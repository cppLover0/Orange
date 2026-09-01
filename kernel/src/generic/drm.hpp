#pragma once

#include <generic/vfs.hpp>
#include <utils/__utypes.hpp>
#include <utils/foreach.hpp>
#include <utils/number_allocator.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <utils/drm_fourcc.hpp>

// ill make it as another fs

struct drm_clip_rect {
	unsigned short x1;
	unsigned short y1;
	unsigned short x2;
	unsigned short y2;
};

namespace drm {

    namespace drm_structs {

        #define DRM_MODE_TYPE_PREFERRED	(1<<3)
        #define DRM_MODE_TYPE_USERDEF	(1<<5)
        #define DRM_MODE_TYPE_DRIVER	(1<<6)

        #define DRM_MODE_PROP_PENDING	(1<<0) 
        #define DRM_MODE_PROP_RANGE	(1<<1)
        #define DRM_MODE_PROP_IMMUTABLE	(1<<2)
        #define DRM_MODE_PROP_ENUM	(1<<3) 
        #define DRM_MODE_PROP_BLOB	(1<<4)
        #define DRM_MODE_PROP_BITMASK	(1<<5) 

        #define DRM_MODE_FLAG_PHSYNC			(1<<0)
        #define DRM_MODE_FLAG_NHSYNC			(1<<1)
        #define DRM_MODE_FLAG_PVSYNC			(1<<2)
        #define DRM_MODE_FLAG_NVSYNC			(1<<3)

        #define DRM_MODE_PROP_LEGACY_TYPE  ( \
                DRM_MODE_PROP_RANGE | \
                DRM_MODE_PROP_ENUM | \
                DRM_MODE_PROP_BLOB | \
                DRM_MODE_PROP_BITMASK)

        #define DRM_MODE_PROP_EXTENDED_TYPE	0x0000ffc0
        #define DRM_MODE_PROP_TYPE(n)		((n) << 6)
        #define DRM_MODE_PROP_OBJECT		DRM_MODE_PROP_TYPE(1)
        #define DRM_MODE_PROP_SIGNED_RANGE	DRM_MODE_PROP_TYPE(2)

        #define DRM_MODE_ENCODER_NONE	0
        #define DRM_MODE_ENCODER_DAC	1
        #define DRM_MODE_ENCODER_TMDS	2
        #define DRM_MODE_ENCODER_LVDS	3
        #define DRM_MODE_ENCODER_TVDAC	4
        #define DRM_MODE_ENCODER_VIRTUAL 5
        #define DRM_MODE_ENCODER_DSI	6
        #define DRM_MODE_ENCODER_DPMST	7
        #define DRM_MODE_ENCODER_DPI	8

        enum class drm_caps {
            DRM_CAP_DUMB_BUFFER = 0x1,
            DRM_CAP_DUMB_PREFERRED_DEPTH = 0x3,
            DRM_CAP_CURSOR_WIDTH = 0x8,
            DRM_CAP_CURSOR_HEIGHT = 0x9,
            DRM_CAP_DUMB_PREFER_SHADOW = 0x4,
            DRM_CAP_PRIME = 0x5,
            DRM_CAP_ADDFB2_MODIFIERS = 0x10,
            DRM_CAP_ASYNC_PAGE_FLIP = 0x07
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
            DRM_IOCTL_SET_CLIENT_CAP = 0x0D,
            DRM_IOCTL_MODE_GETCONNECTOR	= 0xA7,
            DRM_IOCTL_MODE_GETPROPERTY = 0xAA,
            DRM_IOCTL_MODE_GETENCODER = 0xA6,
            DRM_IOCTL_MODE_OBJ_GETPROPERTIES = 0xB9,
            DRM_IOCTL_MODE_GETCRTC = 0xA1,
            DRM_IOCTL_MODE_GETPLANERESOURCES = 0xB5,
            DRM_IOCTL_MODE_GETPLANE = 0xB6,
            DRM_IOCTL_MODE_CURSOR = 0xA3,
            DRM_IOCTL_SET_MASTER = 0x1e,
            DRM_IOCTL_DROP_MASTER = 0x1f,
            DRM_IOCTL_MODE_SETPROPERTY = 0xAB,
            DRM_IOCTL_MODE_LIST_LESSEES	= 0xC7,
            DRM_IOCTL_MODE_DIRTYFB = 0xB1,
            DRM_IOCTL_MODE_SETGAMMA = 0xA5,
            DRM_IOCTL_MODE_SETCRTC = 0xA2,
            DRM_IOCTL_MODE_CURSOR2 = 0xBB
        };

        #define DRM_MODE_OBJECT_CRTC 0xcccccccc
        #define DRM_MODE_OBJECT_CONNECTOR 0xc0c0c0c0
        #define DRM_MODE_OBJECT_ENCODER 0xe0e0e0e0
        #define DRM_MODE_OBJECT_MODE 0xdededede
        #define DRM_MODE_OBJECT_PROPERTY 0xb0b0b0b0
        #define DRM_MODE_OBJECT_FB 0xfbfbfbfb
        #define DRM_MODE_OBJECT_BLOB 0xbbbbbbbb
        #define DRM_MODE_OBJECT_PLANE 0xeeeeeeee
        #define DRM_MODE_OBJECT_COLOROP 0xfafafafa
        #define DRM_MODE_OBJECT_ANY 0

        #define DRM_PROP_NAME_LEN 32

        // structs are copied from libdrm

        struct drm_mode_obj_get_properties {
            __u64 props_ptr;
            __u64 prop_values_ptr;
            __u32 count_props;
            __u32 obj_id;
            __u32 obj_type;
        };

        struct generic_crtc {
            std::uint64_t magic;
            void* props_ptr;
            void* prop_values_ptr;
            std::uint64_t props_count;
        };

        struct drm_mode_fb_dirty_cmd {
            __u32 fb_id;
            __u32 flags;
            __u32 color;
            __u32 num_clips;
            __u64 clips_ptr;
        };

        struct drm_mode_map_dumb {
            /** Handle for the object being mapped. */
            __u32 handle;
            __u32 pad;
            /**
             * Fake offset to use for subsequent mmap call
             *
             * This is a fixed-size type for 32/64 compatibility.
             */
            __u64 offset;
        };

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

        struct drm_mode_list_lessees {
            /**
             * @count_lessees: Number of lessees.
             *
             * On input, provides length of the array.
             * On output, provides total number. No
             * more than the input number will be written
             * back, so two calls can be used to get
             * the size and then the data.
             */
            __u32 count_lessees;
            /** @pad: Padding. */
            __u32 pad;

            /**
             * @lessees_ptr: Pointer to lessees.
             *
             * Pointer to __u64 array of lessee ids
             */
            __u64 lessees_ptr;
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

        struct drm_mode_get_property {

            /** @values_ptr: Pointer to a ``__u64`` array. */
            __u64 values_ptr;
            /** @enum_blob_ptr: Pointer to a struct drm_mode_property_enum array. */
            __u64 enum_blob_ptr;

            /**
            * @prop_id: Object ID of the property which should be retrieved. Set
            * by the caller.
            */
            __u32 prop_id;
            /**
            * @flags: ``DRM_MODE_PROP_*`` bitfield. See &drm_property.flags for
            * a definition of the flags.
            */
            __u32 flags;
            /**
            * @name: Symbolic property name. User-space should use this field to
            * recognize properties.
            */
            char name[DRM_PROP_NAME_LEN];

            /** @count_values: Number of elements in @values_ptr. */
            __u32 count_values;
            /** @count_enum_blobs: Number of elements in @enum_blob_ptr. */
            __u32 count_enum_blobs;

            std::uint64_t magic; 
        };

        struct drm_mode_property_enum {
            __u64 value;
            char name[DRM_PROP_NAME_LEN];
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

        struct drm_mode_crtc {
            __u64 set_connectors_ptr;
            __u32 count_connectors;

            __u32 crtc_id; /**< Id */
            __u32 fb_id; /**< Id of framebuffer */

            __u32 x; /**< x Position on the framebuffer */
            __u32 y; /**< y Position on the framebuffer */

            __u32 gamma_size;
            __u32 mode_valid;
            struct drm_mode_modeinfo mode;
        };

        struct drm_mode_get_encoder {
            __u32 encoder_id;
            __u32 encoder_type;

            __u32 crtc_id; /**< Id of crtc */

            __u32 possible_crtcs;
            __u32 possible_clones;
        };

        struct drm_dumbbuffer {
            std::uint64_t phys;
            std::uint64_t len;
            std::uint64_t magic;
            bool is_fb;
        };

        struct drm_mode_destroy_dumb {
            __u32 handle;
        };

        struct drm_mode_get_plane {
            /**
             * @plane_id: Object ID of the plane whose information should be
             * retrieved. Set by caller.
             */
            __u32 plane_id;

            /** @crtc_id: Object ID of the current CRTC. */
            __u32 crtc_id;
            /** @fb_id: Object ID of the current fb. */
            __u32 fb_id;

            /**
             * @possible_crtcs: Bitmask of CRTC's compatible with the plane. CRTC's
             * are created and they receive an index, which corresponds to their
             * position in the bitmask. Bit N corresponds to
             * :ref:`CRTC index<crtc_index>` N.
             */
            __u32 possible_crtcs;
            /** @gamma_size: Never used. */
            __u32 gamma_size;

            /** @count_format_types: Number of formats. */
            __u32 count_format_types;
            /**
             * @format_type_ptr: Pointer to ``__u32`` array of formats that are
             * supported by the plane. These formats do not require modifiers.
             */
            __u64 format_type_ptr;
        
            std::uint64_t magic; 
        
        };

        struct drm_mode_get_plane_res {
            __u64 plane_id_ptr;
            __u32 count_planes;
        };

        struct drm_mode_connector_set_property {
            __u64 value;
            __u32 prop_id;
            __u32 connector_id;
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
        std::uint32_t* plane_ptr;

        std::uint32_t connector_count;
        std::uint32_t encoder_count;
        std::uint32_t crtc_count;
        std::uint32_t mode_count;
        std::uint32_t plane_count;

        std::uint32_t* plane_props;
        std::uint64_t* plane_prop_values;
        std::uint32_t plane_prop_count;

        std::uint32_t active_fb;
        drm_structs::drm_mode_modeinfo current_mode;

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

    std::uint32_t fb_to_format(framebuffer* fb);

    struct drm_enum {
        std::uint64_t value;
        const char* name; //copied later
    };

    inline static void fill_property(const char* name, std::uint32_t type, drm_structs::drm_mode_get_property* out) {
        klibc::memcpy(out->name, name, klibc::strlen(name) + 1);
        out->flags = type;
        out->prop_id = allocate();
        out->magic = 0x666111;
    }

    inline static void fill_property(const char* name, std::uint32_t type, drm_structs::drm_mode_get_property* out, std::uint64_t* values, std::uint32_t values_size) {
        klibc::memcpy(out->name, name, klibc::strlen(name) + 1);
        out->flags = type;
        out->prop_id = allocate();
        out->values_ptr = (std::uint64_t)(new std::uint64_t[values_size]);
        out->count_values = values_size;
        out->magic = 0x666111;
        klibc::memcpy((void*)out->values_ptr, values, values_size * sizeof(std::uint64_t));
    }

    inline static void fill_property(const char* name, std::uint32_t type, drm_structs::drm_mode_get_property* out, drm_enum* enum1, std::uint32_t count_enum) {
        klibc::memcpy(out->name, name, klibc::strlen(name) + 1);
        out->flags = type;
        out->prop_id = allocate();
        out->enum_blob_ptr = (std::uint64_t)(new drm_structs::drm_mode_property_enum[count_enum]);
        out->count_enum_blobs = count_enum;
        out->magic = 0x666111;

        auto drm_array = (drm_structs::drm_mode_property_enum*)out->enum_blob_ptr;
        for(std::uint32_t i = 0; i < count_enum; i++) {
            drm_array[i].value = enum1[i].value;
            klibc::memcpy(drm_array[i].name, enum1[i].name, klibc::strlen(enum1[i].name) + 1);
        }
    }

}