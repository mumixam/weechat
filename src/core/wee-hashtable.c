/*
 * Copyright (C) 2010-2011 Sebastien Helleu <flashcode@flashtux.org>
 *
 * This file is part of WeeChat, the extensible chat client.
 *
 * WeeChat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * wee-hashtable.c: implementation of hashtable
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "weechat.h"
#include "wee-hashtable.h"
#include "wee-infolist.h"
#include "wee-log.h"
#include "wee-string.h"
#include "../plugins/plugin.h"


char *hashtable_type_string[HASHTABLE_NUM_TYPES] =
{ WEECHAT_HASHTABLE_INTEGER, WEECHAT_HASHTABLE_STRING,
  WEECHAT_HASHTABLE_POINTER, WEECHAT_HASHTABLE_BUFFER,
  WEECHAT_HASHTABLE_TIME };


/*
 * hashtable_get_type: get integer for type (string)
 */

int
hashtable_get_type (const char *type)
{
    int i;
    
    if (!type)
        return -1;
    
    for (i = 0; i < HASHTABLE_NUM_TYPES; i++)
    {
        if (string_strcasecmp (hashtable_type_string[i], type) == 0)
            return i;
    }
    
    /* type not found */
    return -1;
}

/*
 * hashtable_hash_key_string_cb: default callback to hash a string key
 */

unsigned int
hashtable_hash_key_string_cb (struct t_hashtable *hashtable, const void *key)
{
    const char *ptr_key;
    unsigned long hash;
    
    /* variant of djb2 hash */
    hash = 5381;
    for (ptr_key = (const char *)key; ptr_key[0]; ptr_key++)
    {
        hash ^= (hash << 5) + (hash >> 2) + (int)(ptr_key[0]);
    }
    return hash % hashtable->size;
}

/*
 * hashtable_keycmp_string_cb: default callback for string comparison on keys
 */

int
hashtable_keycmp_string_cb (struct t_hashtable *hashtable,
                            const void *key1, const void *key2)
{
    /* make C compiler happy */
    (void) hashtable;
    
    return strcmp ((const char *)key1, (const char *)key2);
}

/*
 * hashtable_new: create a new hash table
 */

struct t_hashtable *
hashtable_new (int size,
               const char *type_keys, const char *type_values,
               t_hashtable_hash_key *callback_hash_key,
               t_hashtable_keycmp *callback_keycmp)
{
    struct t_hashtable *new_hashtable;
    int i, type_keys_int, type_values_int;
    
    type_keys_int = hashtable_get_type (type_keys);
    if (type_keys_int < 0)
        return NULL;
    type_values_int = hashtable_get_type (type_values);
    if (type_values_int < 0)
        return NULL;
    
    if ((type_keys_int != HASHTABLE_STRING) && (!callback_hash_key || !callback_keycmp))
        return NULL;
    
    new_hashtable = malloc (sizeof (*new_hashtable));
    if (new_hashtable)
    {
        new_hashtable->size = size;
        new_hashtable->type_keys = type_keys_int;
        new_hashtable->type_values = type_values_int;
        new_hashtable->htable = malloc (size * sizeof (*(new_hashtable->htable)));
        new_hashtable->keys_values = NULL;
        if (!new_hashtable->htable)
        {
            free (new_hashtable);
            return NULL;
        }
        for (i = 0; i < size; i++)
        {
            new_hashtable->htable[i] = NULL;
        }
        new_hashtable->items_count = 0;
        
        if ((type_keys_int == HASHTABLE_STRING) && !callback_hash_key)
            new_hashtable->callback_hash_key = &hashtable_hash_key_string_cb;
        else
            new_hashtable->callback_hash_key = callback_hash_key;
        
        if ((type_keys_int == HASHTABLE_STRING) && !callback_keycmp)
            new_hashtable->callback_keycmp = &hashtable_keycmp_string_cb;
        else
            new_hashtable->callback_keycmp = callback_keycmp;
        
        new_hashtable->callback_free_value = NULL;
    }
    return new_hashtable;
}

/*
 * hashtable_alloc_type: alloc space for a key or value
 */

void
hashtable_alloc_type (enum t_hashtable_type type,
                      const void *value, int size_value,
                      void **pointer, int *size)
{
    switch (type)
    {
        case HASHTABLE_INTEGER:
            if (value)
            {
                *pointer = malloc (sizeof (int));
                if (*pointer)
                    *((int *)(*pointer)) = *((int *)value);
            }
            else
                *pointer = NULL;
            *size = (*pointer) ? sizeof (int) : 0;
            break;
        case HASHTABLE_STRING:
            *pointer = (value) ? strdup ((const char *)value) : NULL;
            *size = (*pointer) ? strlen (*pointer) + 1 : 0;
            break;
        case HASHTABLE_POINTER:
            *pointer = (void *)value;
            *size = sizeof (void *);
            break;
        case HASHTABLE_BUFFER:
            if (value)
            {
                *pointer = malloc (size_value);
                if (*pointer)
                    memcpy (*pointer, value, size_value);
            }
            else
                *pointer = NULL;
            *size = (*pointer) ? size_value : 0;
            break;
        case HASHTABLE_TIME:
            if (value)
            {
                *pointer = malloc (sizeof (time_t));
                if (*pointer)
                    *((time_t *)(*pointer)) = *((time_t *)value);
            }
            else
                *pointer = NULL;
            *size = (*pointer) ? sizeof (time_t) : 0;
            break;
        case HASHTABLE_NUM_TYPES:
            break;
    }
}

/*
 * hashtable_free_key: free space used by a key
 */

void
hashtable_free_key (struct t_hashtable *hashtable,
                    struct t_hashtable_item *item)
{
    switch (hashtable->type_keys)
    {
        case HASHTABLE_INTEGER:
        case HASHTABLE_STRING:
        case HASHTABLE_BUFFER:
        case HASHTABLE_TIME:
            free (item->key);
            break;
        case HASHTABLE_POINTER:
            break;
        case HASHTABLE_NUM_TYPES:
            break;
    }
}

/*
 * hashtable_free_value: free space used by a value
 */

void
hashtable_free_value (struct t_hashtable *hashtable,
                      struct t_hashtable_item *item)
{
    if (hashtable->callback_free_value)
    {
        (void) (hashtable->callback_free_value) (hashtable,
                                                 item->key,
                                                 item->value);
    }
    else
    {
        switch (hashtable->type_values)
        {
            case HASHTABLE_INTEGER:
            case HASHTABLE_STRING:
            case HASHTABLE_BUFFER:
            case HASHTABLE_TIME:
                free (item->value);
                break;
            case HASHTABLE_POINTER:
                break;
            case HASHTABLE_NUM_TYPES:
                break;
        }
    }
}

/*
 * hashtable_set_with_size: set value for item in hash table
 *                          argument size is used only for type "buffer"
 *                          return 1 if ok, 0 if error
 */

int
hashtable_set_with_size (struct t_hashtable *hashtable,
                         const void *key, int key_size,
                         const void *value, int value_size)
{
    unsigned int hash;
    struct t_hashtable_item *ptr_item, *pos_item, *new_item;
    
    if (!hashtable || !key
        || ((hashtable->type_keys == HASHTABLE_BUFFER) && (key_size <= 0))
        || ((hashtable->type_values == HASHTABLE_BUFFER) && (value_size <= 0)))
    {
        return 0;
    }
    
    /* search position for item in hash table */
    hash = hashtable->callback_hash_key (hashtable, key);
    pos_item = NULL;
    for (ptr_item = hashtable->htable[hash];
         ptr_item
             && ((int)(hashtable->callback_keycmp) (hashtable, key, ptr_item->key) > 0);
         ptr_item = ptr_item->next_item)
    {
        pos_item = ptr_item;
    }
    
    /* replace value if item is already in hash table */
    if (ptr_item && (hashtable->callback_keycmp (hashtable, key, ptr_item->key) == 0))
    {
        hashtable_free_value (hashtable, ptr_item);
        hashtable_alloc_type (hashtable->type_values,
                              value, value_size,
                              &ptr_item->value, &ptr_item->value_size);
        return 1;
    }
    
    /* create new item */
    new_item = malloc (sizeof (*new_item));
    if (!new_item)
        return 0;
    
    /* set key and value */
    hashtable_alloc_type (hashtable->type_keys,
                          key, key_size,
                          &new_item->key, &new_item->key_size);
    hashtable_alloc_type (hashtable->type_values,
                          value, value_size,
                          &new_item->value, &new_item->value_size);
    
    /* add item */
    if (pos_item)
    {
        /* insert item after position found */
        new_item->prev_item = pos_item;
        new_item->next_item = pos_item->next_item;
        if (pos_item->next_item)
            (pos_item->next_item)->prev_item = new_item;
        pos_item->next_item = new_item;
    }
    else
    {
        /* insert item at beginning of list */
        new_item->prev_item = NULL;
        new_item->next_item = hashtable->htable[hash];
        if (hashtable->htable[hash])
            (hashtable->htable[hash])->prev_item = new_item;
        hashtable->htable[hash] = new_item;
    }
    
    hashtable->items_count++;
    
    return 1;
}

/*
 * hashtable_set: set value for item in hash table
 *                return 1 if ok, 0 if error
 *                Note: this function can be called *only* if key AND value are
 *                      *not* of type "buffer"
 */

int
hashtable_set (struct t_hashtable *hashtable,
               const void *key, const void *value)
{
    return hashtable_set_with_size (hashtable, key, 0, value, 0);
}

/*
 * hashtable_get_item: search an item in hashtable
 *                     if hash is non NULL pointer, then it is set with
 *                     hash value of key (even if key is not found)
 */

struct t_hashtable_item *
hashtable_get_item (struct t_hashtable *hashtable, const void *key,
                    unsigned int *hash)
{
    unsigned int key_hash;
    struct t_hashtable_item *ptr_item;
    
    if (!hashtable)
        return NULL;
    
    key_hash = hashtable->callback_hash_key (hashtable, key);
    if (hash)
        *hash = key_hash;
    for (ptr_item = hashtable->htable[key_hash];
         ptr_item && hashtable->callback_keycmp (hashtable, key, ptr_item->key) > 0;
         ptr_item = ptr_item->next_item)
    {
    }
    
    if (ptr_item
        && (hashtable->callback_keycmp (hashtable, key, ptr_item->key) == 0))
    {
        return ptr_item;
    }
    
    return NULL;
}

/*
 * hashtable_get: get value for a key in hash table
 *                return pointer to "value" for key,
 *                or NULL if key is not found in hash table
 */

void *
hashtable_get (struct t_hashtable *hashtable, const void *key)
{
    struct t_hashtable_item *ptr_item;
    
    ptr_item = hashtable_get_item (hashtable, key, NULL);
    
    return (ptr_item) ? ptr_item->value : NULL;
}

/*
 * hashtable_has_key: return 1 if key is in hashtable, otherwise 0
 */

int
hashtable_has_key (struct t_hashtable *hashtable, const void *key)
{
    return (hashtable_get_item (hashtable, key, NULL) != NULL) ? 1 : 0;
}

/*
 * hashtable_map: call a function on all hashtable entries
 */

void
hashtable_map (struct t_hashtable *hashtable,
               t_hashtable_map *callback_map,
               void *callback_map_data)
{
    int i;
    struct t_hashtable_item *ptr_item, *ptr_next_item;
    
    if (!hashtable)
        return;
    
    for (i = 0; i < hashtable->size; i++)
    {
        ptr_item = hashtable->htable[i];
        while (ptr_item)
        {
            ptr_next_item = ptr_item->next_item;
            
            (void) (callback_map) (callback_map_data,
                                   hashtable,
                                   ptr_item->key,
                                   ptr_item->value);
            
            ptr_item = ptr_next_item;
        }
    }
}

/*
 * hashtable_get_integer: get a hashtable property as integer
 */

int
hashtable_get_integer (struct t_hashtable *hashtable, const char *property)
{
    if (hashtable && property)
    {
        if (string_strcasecmp (property, "size") == 0)
            return hashtable->size;
        else if (string_strcasecmp (property, "items_count") == 0)
            return hashtable->items_count;
    }
    
    return 0;
}

/*
 * hashtable_compute_length_keys_cb: compute length of all keys
 */

void
hashtable_compute_length_keys_cb (void *data,
                                  struct t_hashtable *hashtable,
                                  const void *key, const void *value)
{
    char str_value[128];
    int *length;
    
    /* make C compiler happy */
    (void) value;
    
    length = (int *)data;
    
    switch (hashtable->type_keys)
    {
        case HASHTABLE_INTEGER:
            snprintf (str_value, sizeof (str_value), "%d", *((int *)key));
            *length += strlen (str_value) + 1;
            break;
        case HASHTABLE_STRING:
            *length += strlen ((char *)key) + 1;
            break;
        case HASHTABLE_POINTER:
        case HASHTABLE_BUFFER:
            snprintf (str_value, sizeof (str_value), "0x%lx", (long unsigned int)key);
            *length += strlen (str_value) + 1;
            break;
        case HASHTABLE_TIME:
            snprintf (str_value, sizeof (str_value), "%ld", (long)(*((time_t *)key)));
            *length += strlen (str_value) + 1;
            break;
        case HASHTABLE_NUM_TYPES:
            break;
    }
}

/*
 * hashtable_compute_length_values_cb: compute length of all values
 */

void
hashtable_compute_length_values_cb (void *data,
                                    struct t_hashtable *hashtable,
                                    const void *key, const void *value)
{
    char str_value[128];
    int *length;
    
    /* make C compiler happy */
    (void) key;
    
    length = (int *)data;

    if (value)
    {
        switch (hashtable->type_values)
        {
            case HASHTABLE_INTEGER:
                snprintf (str_value, sizeof (str_value), "%d", *((int *)value));
                *length += strlen (str_value) + 1;
                break;
            case HASHTABLE_STRING:
                *length += strlen ((char *)value) + 1;
                break;
            case HASHTABLE_POINTER:
            case HASHTABLE_BUFFER:
                snprintf (str_value, sizeof (str_value), "0x%lx", (long unsigned int)value);
                *length += strlen (str_value) + 1;
                break;
            case HASHTABLE_TIME:
                snprintf (str_value, sizeof (str_value), "%ld", (long)(*((time_t *)value)));
                *length += strlen (str_value) + 1;
                break;
            case HASHTABLE_NUM_TYPES:
                break;
        }
    }
    else
    {
        *length += strlen ("(null)") + 1;
    }
}

/*
 * hashtable_compute_length_keys_values_cb: compute length of all keys + values
 */

void
hashtable_compute_length_keys_values_cb (void *data,
                                         struct t_hashtable *hashtable,
                                         const void *key, const void *value)
{
    hashtable_compute_length_keys_cb (data, hashtable, key, value);
    hashtable_compute_length_values_cb (data, hashtable, key, value);
}

/*
 * hashtable_build_string_keys_cb: build string with all keys
 */

void
hashtable_build_string_keys_cb (void *data,
                                struct t_hashtable *hashtable,
                                const void *key, const void *value)
{
    char str_value[128];
    char *str;
    
    /* make C compiler happy */
    (void) value;
    
    str = (char *)data;
    
    if (str[0])
        strcat (str, ",");
    
    switch (hashtable->type_keys)
    {
        case HASHTABLE_INTEGER:
            snprintf (str_value, sizeof (str_value), "%d", *((int *)key));
            strcat (str, str_value);
            break;
        case HASHTABLE_STRING:
            strcat (str, (char *)key);
            break;
        case HASHTABLE_POINTER:
        case HASHTABLE_BUFFER:
            snprintf (str_value, sizeof (str_value), "0x%lx", (long unsigned int)key);
            strcat (str, str_value);
            break;
        case HASHTABLE_TIME:
            snprintf (str_value, sizeof (str_value), "%ld", (long)(*((time_t *)key)));
            strcat (str, str_value);
            break;
        case HASHTABLE_NUM_TYPES:
            break;
    }
}

/*
 * hashtable_build_string_values_cb: build string with all values
 */

void
hashtable_build_string_values_cb (void *data,
                                  struct t_hashtable *hashtable,
                                  const void *key, const void *value)
{
    char str_value[128];
    char *str;
    
    /* make C compiler happy */
    (void) key;
    
    str = (char *)data;
    
    if (str[0])
        strcat (str, ",");

    if (value)
    {
        switch (hashtable->type_values)
        {
            case HASHTABLE_INTEGER:
                snprintf (str_value, sizeof (str_value), "%d", *((int *)value));
                strcat (str, str_value);
                break;
            case HASHTABLE_STRING:
                strcat (str, (char *)value);
                break;
            case HASHTABLE_POINTER:
            case HASHTABLE_BUFFER:
                snprintf (str_value, sizeof (str_value), "0x%lx", (long unsigned int)value);
                strcat (str, str_value);
                break;
            case HASHTABLE_TIME:
                snprintf (str_value, sizeof (str_value), "%ld", (long)(*((time_t *)value)));
                strcat (str, str_value);
                break;
            case HASHTABLE_NUM_TYPES:
                break;
        }
    }
    else
    {
        strcat (str, "(null)");
    }
}

/*
 * hashtable_build_string_keys_values_cb: build string with all keys + values
 */

void
hashtable_build_string_keys_values_cb (void *data,
                                       struct t_hashtable *hashtable,
                                       const void *key, const void *value)
{
    char str_value[128];
    char *str;
    
    str = (char *)data;
    
    if (str[0])
        strcat (str, ",");
    
    switch (hashtable->type_keys)
    {
        case HASHTABLE_INTEGER:
            snprintf (str_value, sizeof (str_value), "%d", *((int *)key));
            strcat (str, str_value);
            break;
        case HASHTABLE_STRING:
            strcat (str, (char *)key);
            break;
        case HASHTABLE_POINTER:
        case HASHTABLE_BUFFER:
            snprintf (str_value, sizeof (str_value), "0x%lx", (long unsigned int)key);
            strcat (str, str_value);
            break;
        case HASHTABLE_TIME:
            snprintf (str_value, sizeof (str_value), "%ld", (long)(*((time_t *)key)));
            strcat (str, str_value);
            break;
        case HASHTABLE_NUM_TYPES:
            break;
    }
    
    strcat (str, ":");

    if (value)
    {
        switch (hashtable->type_values)
        {
            case HASHTABLE_INTEGER:
                snprintf (str_value, sizeof (str_value), "%d", *((int *)value));
                strcat (str, str_value);
                break;
            case HASHTABLE_STRING:
                strcat (str, (char *)value);
                break;
            case HASHTABLE_POINTER:
            case HASHTABLE_BUFFER:
                snprintf (str_value, sizeof (str_value), "0x%lx", (long unsigned int)value);
                strcat (str, str_value);
                break;
            case HASHTABLE_TIME:
                snprintf (str_value, sizeof (str_value), "%ld", (long)(*((time_t *)value)));
                strcat (str, str_value);
                break;
            case HASHTABLE_NUM_TYPES:
                break;
        }
    }
    else
    {
        strcat (str, "(null)");
    }
}

/*
 * hashtable_get_keys_values: get keys and/or values of hashtable as string
 *                            string has format, one of:
 *                              keys only:     "key1,key2,key3"
 *                              values only:   "value1,value2,value3"
 *                              keys + values: "key1:value1,key2:value2,key3:value3"
 *                            Note: this works only if keys have type "integer",
 *                                  or "string"
 */

const char *
hashtable_get_keys_values (struct t_hashtable *hashtable, int keys, int values)
{
    int length;
    
    if (hashtable->keys_values)
    {
        free (hashtable->keys_values);
        hashtable->keys_values = NULL;
    }
    
    /* first compute length of string */
    length = 0;
    hashtable_map (hashtable,
                   (keys && values) ? &hashtable_compute_length_keys_values_cb :
                   ((keys) ? &hashtable_compute_length_keys_cb :
                    &hashtable_compute_length_values_cb),
                   &length);
    if (length == 0)
        return hashtable->keys_values;
    
    /* build string */
    hashtable->keys_values = malloc (length + 1);
    if (!hashtable->keys_values)
        return NULL;
    hashtable->keys_values[0] = '\0';
    hashtable_map (hashtable,
                   (keys && values) ? &hashtable_build_string_keys_values_cb :
                   ((keys) ? &hashtable_build_string_keys_cb :
                    &hashtable_build_string_values_cb),
                   hashtable->keys_values);
    
    return hashtable->keys_values;
}

/*
 * hashtable_get_string: get a hashtable property as string
 */

const char *
hashtable_get_string (struct t_hashtable *hashtable, const char *property)
{
    if (hashtable && property)
    {
        if (string_strcasecmp (property, "type_keys") == 0)
            return hashtable_type_string[hashtable->type_keys];
        else if (string_strcasecmp (property, "type_values") == 0)
            return hashtable_type_string[hashtable->type_values];
        else if (string_strcasecmp (property, "keys") == 0)
            return hashtable_get_keys_values (hashtable, 1, 0);
        else if (string_strcasecmp (property, "values") == 0)
            return hashtable_get_keys_values (hashtable, 0, 1);
        else if (string_strcasecmp (property, "keys_values") == 0)
            return hashtable_get_keys_values (hashtable, 1, 1);
    }
    
    return NULL;
}

/*
 * hashtable_set_pointer: set a hashtable property (pointer)
 */

void
hashtable_set_pointer (struct t_hashtable *hashtable, const char *property,
                       void *pointer)
{
    if (hashtable && property)
    {
        if (string_strcasecmp (property, "callback_free_value") == 0)
            hashtable->callback_free_value = pointer;
    }
}

/*
 * hashtable_add_to_infolist: add hashtable keys and values to infolist
 *                            return 1 if ok, 0 if error
 */

int
hashtable_add_to_infolist (struct t_hashtable *hashtable,
                           struct t_infolist_item *infolist_item,
                           const char *prefix)
{
    int i, item_number;
    struct t_hashtable_item *ptr_item;
    char option_name[128];
    
    if (!hashtable || (hashtable->type_keys != HASHTABLE_STRING)
        || !infolist_item || !prefix)
        return 0;
    
    item_number = 0;
    for (i = 0; i < hashtable->size; i++)
    {
        for (ptr_item = hashtable->htable[i]; ptr_item;
             ptr_item = ptr_item->next_item)
        {
            snprintf (option_name, sizeof (option_name),
                      "%s_name_%05d", prefix, item_number);
            if (!infolist_new_var_string (infolist_item, option_name,
                                          (const char *)ptr_item->key))
                return 0;
            snprintf (option_name, sizeof (option_name),
                      "%s_value_%05d", prefix, item_number);
            switch (hashtable->type_values)
            {
                case HASHTABLE_INTEGER:
                    if (!infolist_new_var_integer (infolist_item, option_name,
                                                   *((int *)ptr_item->value)))
                        return 0;
                    break;
                case HASHTABLE_STRING:
                    if (!infolist_new_var_string (infolist_item, option_name,
                                                  (const char *)ptr_item->value))
                        return 0;
                    break;
                case HASHTABLE_POINTER:
                    if (!infolist_new_var_pointer (infolist_item, option_name,
                                                   ptr_item->value))
                        return 0;
                    break;
                case HASHTABLE_BUFFER:
                    if (!infolist_new_var_buffer (infolist_item, option_name,
                                                  ptr_item->value,
                                                  ptr_item->value_size))
                        return 0;
                    break;
                case HASHTABLE_TIME:
                    if (!infolist_new_var_time (infolist_item, option_name,
                                                *((time_t *)ptr_item->value)))
                        return 0;
                    break;
                case HASHTABLE_NUM_TYPES:
                    break;
            }
            item_number++;
        }
    }
    return 1;
}

/*
 * hashtable_remove_item: remove an item from hashmap
 */

void
hashtable_remove_item (struct t_hashtable *hashtable,
                       struct t_hashtable_item *item,
                       unsigned int hash)
{
    if (!hashtable || !item)
        return;
    
    /* free key and value */
    hashtable_free_value (hashtable, item);
    hashtable_free_key (hashtable, item);
    
    /* remove item from list */
    if (item->prev_item)
        (item->prev_item)->next_item = item->next_item;
    if (item->next_item)
        (item->next_item)->prev_item = item->prev_item;
    if (hashtable->htable[hash] == item)
        hashtable->htable[hash] = item->next_item;
    
    free (item);
    
    hashtable->items_count--;
}

/*
 * hashtable_remove: remove an item from hashmap (search it with key)
 */

void
hashtable_remove (struct t_hashtable *hashtable, const void *key)
{
    struct t_hashtable_item *ptr_item;
    unsigned int hash;
    
    if (!hashtable || !key)
        return;
    
    ptr_item = hashtable_get_item (hashtable, key, &hash);
    if (ptr_item)
        hashtable_remove_item (hashtable, ptr_item, hash);
}

/*
 * hashtable_remove_all: remove all items from hashmap
 */

void
hashtable_remove_all (struct t_hashtable *hashtable)
{
    int i;
    
    if (!hashtable)
        return;
    
    for (i = 0; i < hashtable->size; i++)
    {
        while (hashtable->htable[i])
        {
            hashtable_remove_item (hashtable, hashtable->htable[i], i);
        }
    }
}

/*
 * hashtable_free: free hashtable (remove all items and free hashtable)
 */

void
hashtable_free (struct t_hashtable *hashtable)
{
    if (!hashtable)
        return;
    
    hashtable_remove_all (hashtable);
    free (hashtable->htable);
    if (hashtable->keys_values)
        free (hashtable->keys_values);
    free (hashtable);
}

/*
 * hashtable_print_log: print hashtable in log (usually for crash dump)
 */

void
hashtable_print_log (struct t_hashtable *hashtable, const char *name)
{
    struct t_hashtable_item *ptr_item;
    int i;
    
    log_printf ("");
    log_printf ("[hashtable %s (addr:0x%lx)]", name, hashtable);
    log_printf ("  size . . . . . . . . . : %d",    hashtable->size);
    log_printf ("  htable . . . . . . . . : 0x%lx", hashtable->htable);
    log_printf ("  items_count. . . . . . : %d",    hashtable->items_count);
    log_printf ("  type_keys. . . . . . . : %d (%s)",
                hashtable->type_keys,
                hashtable_type_string[hashtable->type_keys]);
    log_printf ("  type_values. . . . . . : %d (%s)",
                hashtable->type_values,
                hashtable_type_string[hashtable->type_values]);
    log_printf ("  callback_hash_key. . . : 0x%lx", hashtable->callback_hash_key);
    log_printf ("  callback_keycmp. . . . : 0x%lx", hashtable->callback_keycmp);
    
    for (i = 0; i < hashtable->size; i++)
    {
        log_printf ("  htable[%06d] . . . . : 0x%lx", i, hashtable->htable[i]);
        for (ptr_item = hashtable->htable[i]; ptr_item;
             ptr_item = ptr_item->next_item)
        {
            log_printf ("    [item 0x%lx]", hashtable->htable);
            switch (hashtable->type_keys)
            {
                case HASHTABLE_INTEGER:
                    log_printf ("      key (integer). . . : %d", *((int *)ptr_item->key));
                    break;
                case HASHTABLE_STRING:
                    log_printf ("      key (string) . . . : '%s'", (char *)ptr_item->key);
                    break;
                case HASHTABLE_POINTER:
                    log_printf ("      key (pointer). . . : 0x%lx", ptr_item->key);
                    break;
                case HASHTABLE_BUFFER:
                    log_printf ("      key (buffer) . . . : 0x%lx", ptr_item->key);
                    break;
                case HASHTABLE_TIME:
                    log_printf ("      key (time) . . . . : %ld",   *((time_t *)ptr_item->key));
                    break;
                case HASHTABLE_NUM_TYPES:
                    break;
            }
            log_printf ("      key_size . . . . . : %d", ptr_item->key_size);
            switch (hashtable->type_values)
            {
                case HASHTABLE_INTEGER:
                    log_printf ("      value (integer). . : %d", *((int *)ptr_item->value));
                    break;
                case HASHTABLE_STRING:
                    log_printf ("      value (string) . . : '%s'", (char *)ptr_item->value);
                    break;
                case HASHTABLE_POINTER:
                    log_printf ("      value (pointer). . : 0x%lx", ptr_item->value);
                    break;
                case HASHTABLE_BUFFER:
                    log_printf ("      value (buffer) . . : 0x%lx", ptr_item->value);
                    break;
                case HASHTABLE_TIME:
                    log_printf ("      value (time) . . . : %d", *((time_t *)ptr_item->value));
                    break;
                case HASHTABLE_NUM_TYPES:
                    break;
            }
            log_printf ("      value_size . . . . : %d",    ptr_item->value_size);
            log_printf ("      prev_item. . . . . : 0x%lx", ptr_item->prev_item);
            log_printf ("      next_item. . . . . : 0x%lx", ptr_item->next_item);
        }
    }
    log_printf ("  keys_values. . . . . . : '%s'", hashtable->keys_values);
}
