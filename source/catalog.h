#ifndef WIILINK_CATALOG_H
#define WIILINK_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#define CHANNEL_CUSTOM_TICKET (1u << 0)
#define CHANNEL_CUSTOM_TMD    (1u << 1)

typedef struct {
    const char *name;
    uint16_t content_position;
} PatchDef;

typedef struct {
    uint16_t category_id;
    uint16_t item_id;
    const char *name;
    const char *language;
    const char *region;
    const char *title_id;
    uint16_t latest_version;
    const char *patch_folder;
    const PatchDef *patches;
    uint8_t patch_count;
    const char *const *additional_apps;
    uint8_t additional_app_count;
    uint16_t additional_category;
    uint16_t additional_item;
    uint8_t flags;
} ChannelDef;

typedef struct {
    uint16_t category_id;
    const char *name;
    const char *type;
    const char *network;
    uint16_t first_channel;
    uint16_t channel_count;
} CategoryDef;

extern const ChannelDef g_channels[];
extern const size_t g_channel_count;
extern const CategoryDef g_categories[];
extern const size_t g_category_count;

const CategoryDef *catalog_category(uint16_t category_id);
const ChannelDef *catalog_channel(uint16_t category_id, uint16_t item_id);
const CategoryDef *catalog_category_for_channel(const ChannelDef *channel);

#endif
