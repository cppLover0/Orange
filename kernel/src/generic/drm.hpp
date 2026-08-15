#pragma once

#include <generic/vfs.hpp>
#include <utils/__utypes.hpp>
#include <utils/foreach.hpp>
#include <utils/number_allocator.hpp>

// ill make it as another fs

namespace drm {

    namespace drm_structs {

        #define DRM_MODE_TYPE_PREFERRED	(1<<3)
        #define DRM_MODE_TYPE_USERDEF	(1<<5)
        #define DRM_MODE_TYPE_DRIVER	(1<<6)

        #define DRM_MODE_PROP_ENUM	(1<<3)

        enum class drm_other {
            DRM_CONST_FRAMEBUFFER_HANDLE = 0x112233,
            DRM_CONST_FRAMEBUFFER_OFFSET = 0x123456,
            DRM_CONST_FRAMEBUFFER_ID = 0x123
        };

        enum class drm_caps {
            DRM_CAP_DUMB_BUFFER = 0x1,
            DRM_CAP_DUMB_PREFERRED_DEPTH = 0x3,
            DRM_CAP_CURSOR_WIDTH = 0x8,
            DRM_CAP_CURSOR_HEIGHT = 0x9,
            DRM_CAP_DUMB_PREFER_SHADOW = 0x4,
            DRM_CAP_PRIME = 0x5,
            DRM_CAP_ADDFB2_MODIFIERS = 0x10
        };

        enum class drm_requests {
            DRM_IOCTL_MODE_GETRESOURCES = 0xA0,
            DRM_IOCTL_GET_CAP = 0x0C,
            DRM_IOCTL_MODE_CREATE_DUMB = 0xB2,
            DRM_IOCTL_MODE_MAP_DUMB = 0xB3,
            DRM_IOCTL_MODE_ADDFB = 0xAE,
            DRM_IOCTL_MODE_RMFB = 0xAF,
            DRM_IOCTL_MODE_DESTROY_DUMB = 0xB4,
            DRM_IOCTL_VERSION = 0x00,
            DRM_IOCTL_SET_CLIENT_CAP = 0x0D
        };

        // structs are copied from libdrm

        struct drm_mode_card_res {
            __u64 fb_id_ptr;
            __u64 crtc_id_ptr;
            __u64 connector_id_ptr;
            __u64 encoder_id_ptr;
            __u32 count_fbs;
            __u32 count_crtcs;
            __u32 count_connectors;
            __u32 count_encoders;
            __u32 min_width;
            __u32 max_width;
            __u32 min_height;
            __u32 max_height;
        };

        struct drm_get_cap {
            __u64 capability;
            __u64 value;
        };

        struct drm_mode_create_dumb {
            __u32 height;
            __u32 width;
            __u32 bpp;
            __u32 flags;

            __u32 handle;
            __u32 pitch;
            __u64 size;
        };

        struct drm_mode_fb_cmd {
            __u32 fb_id;
            __u32 width;
            __u32 height;
            __u32 pitch;
            __u32 bpp;
            __u32 depth;
            /* driver specific handle */
            __u32 handle;
        };

        struct drm_version {
            int version_major;	  /**< Major version */
            int version_minor;	  /**< Minor version */
            int version_patchlevel;	  /**< Patch level */
            std::size_t name_len;	  /**< Length of name buffer */
            char *name;	  /**< Name of driver */
            std::size_t date_len;	  /**< Length of date buffer */
            char *date;	  /**< User-space buffer to hold date */
            std::size_t desc_len;	  /**< Length of desc buffer */
            char *desc;	  /**< User-space buffer to hold desc */
        };

        struct drm_mode_get_connector {
            /** @encoders_ptr: Pointer to ``__u32`` array of object IDs. */
            __u64 encoders_ptr;
            /** @modes_ptr: Pointer to struct drm_mode_modeinfo array. */
            __u64 modes_ptr;
            /** @props_ptr: Pointer to ``__u32`` array of property IDs. */
            __u64 props_ptr;
            /** @prop_values_ptr: Pointer to ``__u64`` array of property values. */
            __u64 prop_values_ptr;

            /** @count_modes: Number of modes. */
            __u32 count_modes;
            /** @count_props: Number of properties. */
            __u32 count_props;
            /** @count_encoders: Number of encoders. */
            __u32 count_encoders;

            /** @encoder_id: Object ID of the current encoder. */
            __u32 encoder_id;
            /** @connector_id: Object ID of the connector. */
            __u32 connector_id;
            /**
             * @connector_type: Type of the connector.
             *
             * See DRM_MODE_CONNECTOR_* defines.
             */
            __u32 connector_type;
            /**
             * @connector_type_id: Type-specific connector number.
             *
             * This is not an object ID. This is a per-type connector number. Each
             * (type, type_id) combination is unique across all connectors of a DRM
             * device.
             *
             * The (type, type_id) combination is not a stable identifier: the
             * type_id can change depending on the driver probe order.
             */
            __u32 connector_type_id;

            /**
             * @connection: Status of the connector.
             *
             * See enum drm_connector_status.
             */
            __u32 connection;
            /** @mm_width: Width of the connected sink in millimeters. */
            __u32 mm_width;
            /** @mm_height: Height of the connected sink in millimeters. */
            __u32 mm_height;
            /**
             * @subpixel: Subpixel order of the connected sink.
             *
             * See enum subpixel_order.
             */
            __u32 subpixel;

            /** @pad: Padding, must be zero. */
            __u32 pad;
        };

        struct drm_mode_modeinfo {
            __u32 clock;
            __u16 hdisplay;
            __u16 hsync_start;
            __u16 hsync_end;
            __u16 htotal;
            __u16 hskew;
            __u16 vdisplay;
            __u16 vsync_start;
            __u16 vsync_end;
            __u16 vtotal;
            __u16 vscan;

            __u32 vrefresh;

            __u32 flags;
            __u32 type;
            char name[32];
        };

        struct connector {

        };

    }

    enum class drm_type {
        unknown = 0,
        framebuffer = 1,
        gpu = 2 // ????
    };

    inline const char* drm_type_to_str(drm_type type) {
        switch(type) {
        
        case drm_type::unknown:
            return "unknown";

        case drm_type::framebuffer:
            return "linear framebuffer (vbe/gop)";

        case drm_type::gpu:
            return "gpu";

        default:
            return "unknown";
        };
    } 

    struct framebuffer {
        std::uint64_t phys;
        std::uint64_t width;
        std::uint64_t height;
        std::uint64_t pitch;
        std::uint16_t bpp;
        std::uint8_t memory_model;
        std::uint8_t red_mask_size;
        std::uint8_t red_mask_shift;
        std::uint8_t green_mask_size;
        std::uint8_t green_mask_shift;
        std::uint8_t blue_mask_size;
        std::uint8_t blue_mask_shift;
        std::uint64_t edid_size;
        void* edid;
    };

    struct kms_keytostruct {
        void* ptr;
        std::uint64_t key;
        kms_keytostruct* next;
    };

    class kms_allocator {
    private:
        kms_keytostruct* head = nullptr;
    public:
        
        void* get(std::uint64_t key) {
            foreach(this->head) {
                if(item->key == key)
                    return item->ptr;
            }
            return nullptr;
        }

        void create(std::uint64_t key, void* ptr) {
            auto new_kms = new kms_keytostruct;
            new_kms->key = key;
            new_kms->ptr = ptr;

            new_kms->next = this->head;
            this->head = new_kms;
        }

    };

    struct drm_device {

        std::int64_t magic;

        drm_type type;
        void* ctx;

        int inode;
        bool is_root;

        std::uint32_t* connector_ptr;
        std::uint32_t* encoder_ptr;
        std::uint32_t* crtc_ptr;
        std::uint32_t* mode_ptr;

        std::uint32_t connector_count;
        std::uint32_t encoder_count;
        std::uint32_t crtc_count;
        std::uint32_t mode_count;

        struct {
            framebuffer (*access_fb)(void* ctx);
        } fb;

        char path[256];

        drm_device* next;
    };

    // kms is kernel mode setting, not what you think
    inline std::atomic<std::uint32_t> kms_id_ptr = 1;

    inline std::uint32_t allocate() {
        return kms_id_ptr.fetch_add(1);
    }

    inline utils::number_allocator<std::uint32_t> unknown_connector_type_allocator;
    inline kms_allocator _kms;

    void init(vfs::node* node);
    void create(drm_device device);

}