// Copyright (c) 2023 licktheroom //

/*
    A template API thing. I don't know the proper term.

    You can use OpenGL or Vulkan. By default Vulkan is used.

    Assuming Vulkan was chosen, the app will:
    Try to load Vulkan,
    If that failed, try to load OpenGL.

    The same is true if OpenGL was chosen:
    Try to load OpenGL,
    If that failed, try to load Vulkan.

    You can force an API which will make the app throw an error instead of trying the other API first.
    All API loading is done in init()
*/

// DEFINES //

#define WN_NAME "xcb-multi"

// HEADERS //

// STANDARD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

// X11

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xcb/xcb.h>

// OPENGL

#include <GL/gl.h>
#include <GL/glx.h>

// VULKAN

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>

// ENUM //

typedef enum {
    GRAPHICS_API_VULKAN = 1,
    GRAPHICS_API_OPENGL = 2,
} graphics_api_e;

// STATIC VARIABLES //

static struct 
{
    struct {
        Display *display;
    } xlib;

    struct
    {
        xcb_connection_t *connection;
        xcb_window_t window;

        xcb_atom_t close_event;
    } xcb;

    struct {
        GLXContext context;
        GLXDrawable drawable;
        GLXWindow window;
    } gl;

    struct {
        VkInstance instance;
        VkSurfaceKHR surface;
        VkPhysicalDevice physical_device;
        VkDevice device;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR surface_mode;
        VkExtent2D ex;
        VkSurfaceCapabilitiesKHR surface_cap;
        VkQueue gp_queue, pr_queue;
        VkSwapchainKHR swap;
        VkRenderPass render_pass;
        VkPipelineLayout pipeline_layout;
        VkPipeline pipeline;
        VkCommandPool cmdpool;
        VkCommandBuffer *cmdbuffer;

        VkImage *images;
        VkImageView *views;
        VkFramebuffer *framebuffers;

        VkSemaphore *img_available;
        VkSemaphore *render_finished;
        VkFence *flight;

        unsigned int image_c;
        unsigned int current_frame;
        unsigned int max_frames;
    } vk;

    bool should_close;

    bool gpu_api_is_forced;
    graphics_api_e gpu_api;

    struct
    {
        int width, height;
    } window;
} game;

const static char *VK_ext[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_XCB_SURFACE_EXTENSION_NAME
};

const static unsigned int VK_ext_c = 2;

const static char *VK_layer[] = {
    "VK_LAYER_KHRONOS_validation"
};

const static unsigned int VK_layer_c = 1;

const static char *VK_dev_ext[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const static unsigned int VK_dev_ext_c = 1;

// FUNCTIONS //

void
clean_up(void);

bool 
init(void);

void
input(void);

// XCB

void
window_error_print(xcb_generic_error_t *error);

bool
window_get_close_event(void);

// OPENGL

bool
init_opengl(void);

void
render_opengl(void);

bool
window_create_opengl(void);

// VULKAN

bool
init_vulkan(void);

void
render_vulkan(void);

bool
window_create_vulkan(void);

void
vk_error_print(VkResult error);

void
vk_read_file(const char *file_name, size_t *size, char **out);

bool
vk_supports_validation_layers(void);

bool
vk_create_instance(void);

bool
vk_create_window_surface(void);

bool
vk_get_queue_families(
    VkPhysicalDevice device,
    unsigned int *gp_family,
    unsigned int *pr_family
);

bool
vk_get_physical_device(void);

bool
vk_create_logic_device(void);

bool
vk_recreate_swapchain(void);

bool
vk_create_swapchain(void);

bool
vk_create_image_views(void);

bool
vk_create_render_pass(void);

bool
vk_create_graphics_pipeline(void);

bool
vk_create_framebuffers(void);

bool
vk_create_cmd_pool(void);

bool
vk_create_cmd_buffer(void);

bool
vk_create_sync_objects(void);

// MAIN //

int
main(int argc, char **argv)
{
    // Set basic window data
    game.window.width = game.window.height = 300;
    game.should_close = false;

    game.gpu_api = GRAPHICS_API_VULKAN;
    game.gpu_api_is_forced = false;
    game.vk.max_frames = 2;
    game.vk.current_frame = 0;

    // Handle arguments
    for(int i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "--use-opengl") == 0) {
            game.gpu_api = GRAPHICS_API_OPENGL;
        } else if(strcmp(argv[i], "--force-opengl") == 0) {
            game.gpu_api = GRAPHICS_API_OPENGL;
            game.gpu_api_is_forced = true;
        } else if(strcmp(argv[i], "--use-vulkan") == 0) {
            game.gpu_api = GRAPHICS_API_VULKAN;
        } else if(strcmp(argv[i], "--force-vulkan") == 0) {
            game.gpu_api = GRAPHICS_API_VULKAN;
            game.gpu_api_is_forced = true;
        } else if(strcmp(argv[i], "--vulkan-max-frames-in-flight") == 0) {
            if(argc >= i + 1) {
                game.vk.max_frames = (unsigned int)strtol(argv[i + 1], 
                                                            (char **)NULL, 
                                                            10);

                if(game.vk.max_frames == 0) {
                    fprintf(stderr, 
                            "Unknown number, "
                            "failed to change max frames in flight!\n");

                    game.vk.max_frames = 2;
                }
            } else {
                fprintf(stderr, 
                        "Wasn't given anything, "
                        "failed to change max frames in flight!\n");
            }
        }
    }

    // Init
    if(!init()) {
        clean_up();
        return -1;
    }
    
    // Wait until we should close
    while(game.should_close == false)
    {
        input();

        if(game.gpu_api == GRAPHICS_API_OPENGL)
            render_opengl();
        else if(game.gpu_api == GRAPHICS_API_VULKAN)
            render_vulkan();
    }

    if(game.gpu_api == GRAPHICS_API_VULKAN)
        vkDeviceWaitIdle(game.vk.device);
    
    clean_up();
    return 0;
}

// FUNCTIONS //

void
clean_up(void)
{
    fprintf(stdout, "Exiting.\n");

    if(game.gpu_api == GRAPHICS_API_OPENGL) {
        glXDestroyWindow(game.xlib.display, game.gl.window);
        xcb_destroy_window(game.xcb.connection, game.xcb.window);
        glXDestroyContext(game.xlib.display, game.gl.context);
        XCloseDisplay(game.xlib.display);
    } else if(game.gpu_api == GRAPHICS_API_VULKAN) {

        for(unsigned int i = 0; i < game.vk.max_frames; i++)
        {
            vkDestroyFence(game.vk.device, game.vk.flight[i], NULL);
            vkDestroySemaphore(game.vk.device, 
                               game.vk.render_finished[i], 
                               NULL);

            vkDestroySemaphore(game.vk.device, game.vk.img_available[i], NULL);
        }

        free(game.vk.flight);
        free(game.vk.render_finished);
        free(game.vk.img_available);

        vkDestroyCommandPool(game.vk.device, game.vk.cmdpool, NULL);

        for(unsigned int i = 0; i < game.vk.image_c; i++)
            vkDestroyFramebuffer(game.vk.device, 
                                 game.vk.framebuffers[i], 
                                 NULL);
        
        free(game.vk.framebuffers);

        vkDestroyPipeline(game.vk.device, game.vk.pipeline, NULL);
        vkDestroyPipelineLayout(game.vk.device, game.vk.pipeline_layout, NULL);
        vkDestroyRenderPass(game.vk.device, game.vk.render_pass, NULL);

        for(unsigned int i = 0; i < game.vk.image_c; i++)
            vkDestroyImageView(game.vk.device, game.vk.views[i], NULL);
        
        free(game.vk.views);
        free(game.vk.images);

        vkDestroySwapchainKHR(game.vk.device, game.vk.swap, NULL);
        vkDestroyDevice(game.vk.device, NULL);
        vkDestroySurfaceKHR(game.vk.instance, game.vk.surface, NULL);
        vkDestroyInstance(game.vk.instance, NULL);

        xcb_destroy_window(game.xcb.connection, game.xcb.window);
        xcb_disconnect(game.xcb.connection);

    }
}

bool
init(void)
{
    if(game.gpu_api == GRAPHICS_API_OPENGL) {
        bool success = init_opengl();

        if(!success && !game.gpu_api_is_forced) {
            fprintf(stdout, "Failed to load OpenGL, using Vulkan!\n");
            if(!init_vulkan()) {
                fprintf(stderr, "\nInitialization failed!\n");
                return false;
            }
        } else if(!success) {
            fprintf(stderr, "\nInitialization failed!\n");
            return false;
        }
    } else if(game.gpu_api == GRAPHICS_API_VULKAN) {
        bool success = init_vulkan();

        if(!success && !game.gpu_api_is_forced) {
            fprintf(stdout, "Failed to load Vulkan, using OpenGL!\n");
            if(!init_opengl()) {
                fprintf(stderr, "\nInitialization failed!\n");
                return false;
            }
        } else if(!success) {
            fprintf(stderr, "\nInitialization failed!\n");
            return false;
        }
    }

    fprintf(stdout, "Initialization finished.\n");

    return true;
}

void
input(void)
{
    while(true)
    {
        xcb_generic_event_t *event = xcb_poll_for_event(game.xcb.connection);

        if(event == NULL)
            break;

        switch(event->response_type & ~0x80)
        {
            // if it's a message about our window 
            case XCB_CLIENT_MESSAGE:
            {
                unsigned int ev = 
                        (*(xcb_client_message_event_t *)event).data.data32[0];

                // if it's the close event
                if(ev == game.xcb.close_event)
                    game.should_close = true;
            }

            break;

            case XCB_CONFIGURE_NOTIFY:
            {
                int new_width = (*(xcb_configure_notify_event_t *)event).width;
                int new_height = 
                            (*(xcb_configure_notify_event_t *)event).height;

                if(new_height != game.window.height || 
                   new_width != game.window.width) {
                    game.window.width = new_width;
                    game.window.height = new_height;

                    if(game.gpu_api == GRAPHICS_API_OPENGL)
                        glViewport(0, 0, game.window.width, game.window.height);
                   } else if(game.gpu_api == GRAPHICS_API_VULKAN) {
                        if(!vk_recreate_swapchain())
                            game.should_close = true;
                   }
            }

            break;
        }

        free(event); // Always free your event!
    }
}

// As far as I could find, there are very few recources on XCB error codes
// All I can say is look through xproto.h or pray google finds it
void
window_error_print(xcb_generic_error_t *error)
{
    if(error == NULL)
        return;

    const char *names[] = {
        "XCB_REQUEST",
        "XCB_VALUE",
        "XCB_WINDOW",
        "XCB_PIXMAP",
        "XCB_ATOM",
        "XCB_CURSOR",
        "XCB_FONT",
        "XCB_MATCH",
        "XCB_DRAWABLE",
        "XCB_ACCESS",
        "XCB_ALLOC",
        "XCB_COLORMAP",
        "XCB_G_CONTEXT",
        "XCB_ID_CHOICE",
        "XCB_NAME",
        "XCB_LENGTH",
        "XCB_IMPLEMENTATION"
    };

    switch(error->error_code)
    {
        case XCB_REQUEST:
        case XCB_MATCH:
        case XCB_ACCESS:
        case XCB_ALLOC:
        case XCB_NAME:
        case XCB_LENGTH:
        case XCB_IMPLEMENTATION:
            {
                xcb_request_error_t *er = (xcb_request_error_t *)error;

                fprintf(stderr, "REQUEST ERROR\n"
                                "%s\n"
                                "error_code: %d\n"
                                "major: %d\n"
                                "minor: %d\n",
                                names[er->error_code],
                                er->error_code,
                                er->major_opcode,
                                er->minor_opcode);
            }
            break;

        case XCB_VALUE:
        case XCB_WINDOW:
        case XCB_PIXMAP:
        case XCB_ATOM:
        case XCB_CURSOR:
        case XCB_FONT:
        case XCB_DRAWABLE:
        case XCB_COLORMAP:
        case XCB_G_CONTEXT:
        case XCB_ID_CHOICE:
            {
                xcb_value_error_t *er = (xcb_value_error_t *)error;

                fprintf(stderr, "VALUE ERROR\n"
                                "%s\n"
                                "error_code: %d\n"
                                "major: %d\n"
                                "minor: %d\n",
                                names[er->error_code],
                                er->error_code,
                                er->major_opcode,
                                er->minor_opcode);
            }
            break;
    }
}

bool
window_get_close_event(void)
{
    xcb_generic_error_t *error;

    // We need to get WM_PROTOCOLS before we can get the close event
    xcb_intern_atom_cookie_t c_proto = xcb_intern_atom(game.xcb.connection,
                                                       1, 12,
                                                       "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *r_proto = 
                                    xcb_intern_atom_reply(game.xcb.connection,
                                                          c_proto,
                                                          &error);

    // Check for error
    if(error != NULL)
    {
        fprintf(stderr, "Failed to get WM_PROTOCOLS!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Get the close event
    xcb_intern_atom_cookie_t c_close = xcb_intern_atom(game.xcb.connection,
                                                       0, 16,
                                                       "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *r_close = 
                                    xcb_intern_atom_reply(game.xcb.connection,
                                                          c_close,
                                                          &error);

    // Check for error
    if(error != NULL)
    {
        fprintf(stderr, "Failed to get WM_DELETE_WINDOW!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Enable the close event so we can actually receive it
    xcb_void_cookie_t cookie = xcb_change_property(game.xcb.connection,
                                                   XCB_PROP_MODE_REPLACE,
                                                   game.xcb.window,
                                                   r_proto->atom,
                                                   XCB_ATOM_ATOM,
                                                   32,
                                                   1,
                                                   &r_close->atom);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to get XCB window close event!\n");

        window_error_print(error);

        free(error);
        return false;
    }


    game.xcb.close_event = r_close->atom;

    return true;
}

bool
init_opengl(void)
{
    fprintf(stdout, "Loading game with OpenGL.\n");

    if(!window_create_opengl() ||
       !window_get_close_event())
        return false;

    glViewport(0, 0, game.window.width, game.window.height);

    return true;
}

void
render_opengl()
{
    // Clear the buffer
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // We'd do our drawing here 

    // Swap buffers
    glXSwapBuffers(game.xlib.display, game.gl.drawable);
}

// See https://xcb.freedesktop.org/tutorial/basicwindowsanddrawing/
// and https://xcb.freedesktop.org/tutorial/events/
// and https://xcb.freedesktop.org/windowcontextandmanipulation/
bool
window_create_opengl(void)
{
    // Create the connection
    game.xlib.display = XOpenDisplay(0);
    if(!game.xlib.display) {
        fprintf(stderr, "Failed to open X display!\n");

        return false;
    }

    game.xcb.connection = XGetXCBConnection(game.xlib.display);

    if(!game.xcb.connection) {
        fprintf(stderr, "Failed to create connection to Xorg!\n");

        return false;
    }

    XSetEventQueueOwner(game.xlib.display, XCBOwnsEventQueue);

    // Get screen
    int def_screen = DefaultScreen(game.xlib.display);

    const xcb_setup_t *setup = xcb_get_setup(game.xcb.connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for(int screen_num = def_screen;
        iter.rem && screen_num > 0;
        screen_num--, xcb_screen_next(&iter));

    xcb_screen_t *screen = iter.data;

    // Find GL FB
    int vis_id = 0;
    int num_fb_configs = 0;

    const int vis_attribs[] = {
		GLX_X_RENDERABLE, True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		GLX_DOUBLEBUFFER, True,
	};

    const GLXFBConfig *fb_configs = glXChooseFBConfig(game.xlib.display,
                                                      def_screen,
                                                      vis_attribs,
                                                      &num_fb_configs);

    if(!fb_configs || num_fb_configs == 0) {
        fprintf(stderr, "Failed to find an OpenGL FB!\n");

        return false;
    }

    GLXFBConfig fb_config = fb_configs[0];
    glXGetFBConfigAttrib(game.xlib.display, fb_config, GLX_VISUAL_ID, &vis_id);

    // Create GLX context

    game.gl.context = glXCreateNewContext(game.xlib.display,
                                           fb_config,
                                           GLX_RGBA_TYPE,
                                           0,
                                           True);

    if(!game.gl.context) {
        fprintf(stderr, "Failed to create an OpenGL context!\n");

        return false;
    }

    // Error data, used later
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    // Create colormap
    xcb_colormap_t colormap = xcb_generate_id(game.xcb.connection);

    cookie = xcb_create_colormap_checked(game.xcb.connection,
                                         XCB_COLORMAP_ALLOC_NONE,
                                         colormap,
                                         screen->root,
                                         vis_id);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to create XCB colormap!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Create window
    game.xcb.window = xcb_generate_id(game.xcb.connection);

    const int eventmask = XCB_EVENT_MASK_EXPOSURE | 
                          XCB_EVENT_MASK_KEY_PRESS | 
                          XCB_EVENT_MASK_KEY_RELEASE | 
                          XCB_EVENT_MASK_BUTTON_PRESS | 
                          XCB_EVENT_MASK_BUTTON_RELEASE | 
                          XCB_EVENT_MASK_POINTER_MOTION | 
                          XCB_EVENT_MASK_BUTTON_MOTION |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    const int valwin[] = {eventmask, colormap, 0};
    const int valmask = XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

    cookie = xcb_create_window_checked(game.xcb.connection,
                                       XCB_COPY_FROM_PARENT,
                                       game.xcb.window,
                                       screen->root,
                                       0, 0,
                                       game.window.width, game.window.height,
                                       10,
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       screen->root_visual,
                                       valmask, valwin);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to create XCB window!\n");

        window_error_print(error);

        free(error);
        return false;
    }
    
    // Map the window
    cookie = xcb_map_window_checked(game.xcb.connection, game.xcb.window);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to map XCB window!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Start GLX
    game.gl.window = glXCreateWindow(game.xlib.display, 
                                     fb_config, 
                                     game.xcb.window, 
                                     0);

    if(!game.xcb.window) {
        fprintf(stderr, "Failed to create XCB window!\n");
        
        return false;
    }

    game.gl.drawable = game.gl.window;

    int success = glXMakeContextCurrent(game.xlib.display,
                                       game.gl.drawable,
                                       game.gl.drawable,
                                       game.gl.context);

    if(!success) {
        fprintf(stderr, "Failed to make OpenGL current!\n");

        return false;
    }

    // Set the window's name
    cookie = xcb_change_property_checked(game.xcb.connection,
                                         XCB_PROP_MODE_REPLACE,
                                         game.xcb.window,
                                         XCB_ATOM_WM_NAME,
                                         XCB_ATOM_STRING,
                                         8,
                                         strlen(WN_NAME),
                                         WN_NAME);

    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL) {
        fprintf(stderr, "Failed to rename window!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    return true;
}

bool
init_vulkan(void)
{
    fprintf(stdout, "Loading game with Vulkan.\n");

    if(
        !window_create_vulkan()          ||
        !window_get_close_event()        ||
        !vk_supports_validation_layers() ||
        !vk_create_instance()            ||
        !vk_create_window_surface()      ||
        !vk_get_physical_device()        ||
        !vk_create_logic_device()        ||
        !vk_create_swapchain()           ||
        !vk_create_image_views()         ||
        !vk_create_render_pass()         ||
        !vk_create_graphics_pipeline()   ||
        !vk_create_framebuffers()        ||
        !vk_create_cmd_pool()            ||
        !vk_create_cmd_buffer()          ||
        !vk_create_sync_objects()
    ) {
        return false;
    }

    return true;
}

void
render_vulkan(void)
{
    // Wait for previous frame to finish
    vkWaitForFences(game.vk.device, 
                    1, 
                    &game.vk.flight[game.vk.current_frame], 
                    VK_TRUE, 
                    UINT64_MAX);

    // See which image we are using
    unsigned int img_index;
    VkResult success = vkAcquireNextImageKHR(game.vk.device, 
                                             game.vk.swap, 
                                             UINT64_MAX, 
                                game.vk.img_available[game.vk.current_frame], 
                                             VK_NULL_HANDLE, 
                                             &img_index);

    if(success == VK_ERROR_OUT_OF_DATE_KHR) {
        if(!vk_recreate_swapchain())
            game.should_close = true;
        return;
    } else if(success != VK_SUCCESS && success != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to get next image!\n");
        vk_error_print(success);
    
        game.should_close = true;
        return;
    }

    vkResetFences(game.vk.device, 1, &game.vk.flight[game.vk.current_frame]);

    vkResetCommandBuffer(game.vk.cmdbuffer[game.vk.current_frame], 0);

    // Command buffer data
    // This is were we submit our draw calls

    const VkCommandBufferBeginInfo info_b = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

    success = vkBeginCommandBuffer(game.vk.cmdbuffer[game.vk.current_frame], 
                                   &info_b);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to begin recording to the command buffer!\n"
                        "Render failed!\n");
        vk_error_print(success);

        game.should_close = true;
        return;
    }

    const VkClearValue clear_color = {{{0.0f, 1.0f, 0.0f, 1.0f}}};
    const VkRenderPassBeginInfo info_r = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = game.vk.render_pass,
        .framebuffer = game.vk.framebuffers[img_index],
        .renderArea = {

            .offset = {
                .x = 0,
                .y = 0
            },

            .extent = game.vk.ex
        },

        .clearValueCount = 1,
        .pClearValues = &clear_color
    };

    vkCmdBeginRenderPass(game.vk.cmdbuffer[game.vk.current_frame], 
                         &info_r, 
                         VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(game.vk.cmdbuffer[game.vk.current_frame], 
                          VK_PIPELINE_BIND_POINT_GRAPHICS, 
                          game.vk.pipeline);
        
        const VkViewport view = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)game.vk.ex.width,
            .height = (float)game.vk.ex.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(game.vk.cmdbuffer[game.vk.current_frame], 
                         0, 
                         1, 
                         &view);

        const VkRect2D scissor = {
            .offset = {
                .x = 0,
                .y = 0
            },

            .extent = game.vk.ex,
        };
        vkCmdSetScissor(game.vk.cmdbuffer[game.vk.current_frame], 
                        0, 
                        1, 
                        &scissor);

        vkCmdDraw(game.vk.cmdbuffer[game.vk.current_frame], 0, 0, 0, 0);

    vkCmdEndRenderPass(game.vk.cmdbuffer[game.vk.current_frame]);

    success = vkEndCommandBuffer(game.vk.cmdbuffer[game.vk.current_frame]);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to record command buffer!\n"
                        "Render failed!\n");
        vk_error_print(success);

        game.should_close = true;
        return;
    }

    // Submit the command buffer

    VkSemaphore wait[] = {game.vk.img_available[game.vk.current_frame]};
    VkPipelineStageFlags waitf[] = 
                            {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal[] = {game.vk.render_finished[game.vk.current_frame]};

    const VkSubmitInfo info_s = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait,
        .pWaitDstStageMask = waitf,
        .commandBufferCount = 1,
        .pCommandBuffers = &game.vk.cmdbuffer[game.vk.current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal
    };

    success = vkQueueSubmit(game.vk.gp_queue, 
                            1, 
                            &info_s, 
                            game.vk.flight[game.vk.current_frame]);
    
    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw command!\n"
                        "Render failed!\n");
        vk_error_print(success);
        
        game.should_close = true;
        return;
    }

    // Show the image

    VkSwapchainKHR chains[] = {game.vk.swap};

    const VkPresentInfoKHR info_p = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal,
        .swapchainCount = 1,
        .pSwapchains = chains,
        .pImageIndices = &img_index
    };

    success = vkQueuePresentKHR(game.vk.pr_queue, &info_p);

    if(success == VK_ERROR_OUT_OF_DATE_KHR ||
       success == VK_SUBOPTIMAL_KHR) {
        if(!vk_recreate_swapchain())
            game.should_close = true;
    } else if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to get next image!\n");
        vk_error_print(success);

        game.should_close = true;
    }

    game.vk.current_frame = (game.vk.current_frame + 1) & game.vk.max_frames;
}

bool
window_create_vulkan(void)
{
    // Create the connection
    game.xcb.connection = xcb_connect(NULL, NULL);

    if(!game.xcb.connection) {
        fprintf(stderr, "Failed to create connection to Xorg!\n");

        return false;
    }

    // Error data, used later
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    // Get screen
    const xcb_setup_t *setup = xcb_get_setup(game.xcb.connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_screen_t *screen = iter.data;

    // Create window
    const int eventmask = XCB_EVENT_MASK_EXPOSURE | 
                          XCB_EVENT_MASK_KEY_PRESS | 
                          XCB_EVENT_MASK_KEY_RELEASE | 
                          XCB_EVENT_MASK_BUTTON_PRESS | 
                          XCB_EVENT_MASK_BUTTON_RELEASE | 
                          XCB_EVENT_MASK_POINTER_MOTION | 
                          XCB_EVENT_MASK_BUTTON_MOTION |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    const int valwin[] = {eventmask, 0};
    const int valmask = XCB_CW_EVENT_MASK;

    game.xcb.window = xcb_generate_id(game.xcb.connection);

    cookie = xcb_create_window_checked(game.xcb.connection,
                                       XCB_COPY_FROM_PARENT,
                                       game.xcb.window,
                                       screen->root,
                                       0, 0,
                                       game.window.width, game.window.height,
                                       10,
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       screen->root_visual,
                                       valmask, valwin);
    // Check if there was an error
    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL)
    {
        fprintf(stderr, "Failed to create window!\n");

        window_error_print(error);

        free(error);
        return false;
    }
    
    // Map the window
    cookie = xcb_map_window_checked(game.xcb.connection, game.xcb.window);

    // Check for error
    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL)
    {
        fprintf(stderr, "Failed to map window!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    // Set the window's name
    cookie = xcb_change_property_checked(game.xcb.connection,
                                         XCB_PROP_MODE_REPLACE,
                                         game.xcb.window,
                                         XCB_ATOM_WM_NAME,
                                         XCB_ATOM_STRING,
                                         8,
                                         strlen(WN_NAME),
                                         WN_NAME);

    // Check for error
    error = xcb_request_check(game.xcb.connection, cookie);

    if(error != NULL)
    {
        fprintf(stderr, "Failed to rename window!\n");

        window_error_print(error);

        free(error);
        return false;
    }

    return true;
}

void
vk_error_print(VkResult error)
{
    switch(error)
    {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            fprintf(stderr, "VK_ERROR_OUT_OF_HOST_MEMORY\n");
            break;

        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            fprintf(stderr, "VK_ERROR_OUT_OF_DEVICE_MEMORY\n");
            break;

        case VK_ERROR_INITIALIZATION_FAILED:
            fprintf(stderr, "VK_ERROR_INITIALIZATION_FAILED\n");
            break;

        case VK_ERROR_DEVICE_LOST:
            fprintf(stderr, "VK_ERROR_DEVICE_LOST\n");
            break;

        case VK_ERROR_MEMORY_MAP_FAILED:
            fprintf(stderr, "VK_ERROR_MEMORY_MAP_FAILED\n");
            break;

        case VK_ERROR_LAYER_NOT_PRESENT:
            fprintf(stderr, "VK_ERROR_LAYER_NOT_PRESENT\n");
            break;

        case VK_ERROR_EXTENSION_NOT_PRESENT:
            fprintf(stderr, "VK_ERROR_EXTENSION_NOT_PRESENT\n");
            break;

        case VK_ERROR_FEATURE_NOT_PRESENT:
            fprintf(stderr, "VK_ERROR_FEATURE_NOT_PRESENT\n");
            break;

        case VK_ERROR_INCOMPATIBLE_DRIVER:
            fprintf(stderr, "VK_ERROR_FEATURE_NOT_PRESENT\n");
            break;

        case VK_ERROR_TOO_MANY_OBJECTS:
            fprintf(stderr, "VK_ERROR_TOO_MANY_OBJECTS\n");
            break;

        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            fprintf(stderr, "VK_ERROR_FORMAT_NOT_SUPPORTED\n");
            break;

        case VK_ERROR_FRAGMENTED_POOL:
            fprintf(stderr, "VK_ERROR_FRAGMENTED_POOL\n");
            break;

        case VK_ERROR_UNKNOWN:
            fprintf(stderr, "VK_ERROR_UNKNOWN\n");
            break;

        case VK_ERROR_OUT_OF_POOL_MEMORY:
            fprintf(stderr, "VK_ERROR_OUT_OF_POOL_MEMORY\n");
            break;

        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            fprintf(stderr, "VK_ERROR_INVALID_EXTERNAL_HANDLE\n");
            break;

        case VK_ERROR_FRAGMENTATION:
            fprintf(stderr, "VK_ERROR_FRAGMENTATION\n");
            break;

        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            fprintf(stderr, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS\n");
            break;

        case VK_ERROR_SURFACE_LOST_KHR:
            fprintf(stderr, "VK_ERROR_SURFACE_LOST_KHR\n");
            break;

        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            fprintf(stderr, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR\n");
            break;

        case VK_ERROR_OUT_OF_DATE_KHR:
            fprintf(stderr, "VK_ERROR_OUT_OF_DATE_KHR\n");
            break;

        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            fprintf(stderr, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR\n");
            break;

        case VK_ERROR_VALIDATION_FAILED_EXT:
            fprintf(stderr, "VK_ERROR_VALIDATION_FAILED_EXT\n");
            break;

        case VK_ERROR_INVALID_SHADER_NV:
            fprintf(stderr, "VK_ERROR_INVALID_SHADER_NV\n");
            break;

#ifdef VK_ENABLE_BETA_EXTENSIONS

        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
            fprintf(stderr, "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR\n");
            break;

        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
            fprintf(stderr, 
                    "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR\n");
            break;

        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
            fprintf(stderr, 
                    "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR\n");
            break;

        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
            fprintf(stderr, 
                    "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR\n");
            break;

        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
            fprintf(stderr, 
                    "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR\n");
            break;

        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
            fprintf(stderr, "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR\n");
            break;

#endif

        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
            fprintf(stderr, 
                    "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT\n");
            break;

        case VK_ERROR_NOT_PERMITTED_KHR:
            fprintf(stderr, "VK_ERROR_NOT_PERMITTED_KHR\n");
            break;

        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            fprintf(stderr, "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT\n");
            break;

        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
            fprintf(stderr, "VK_ERROR_COMPRESSION_EXHAUSTED_EXT\n");
            break;

        default:
            break;
    }
}

void
vk_read_file(const char *file_name, size_t *size, char **out)
{
    // Open the file and make sure it opened
    FILE *file = fopen(file_name, "rb");
    if(file == NULL) {
        fprintf(stderr, "File '%s' failed to open!\n"
                        "%s\n", 
                        file_name, strerror(errno));

        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);

    // Read the file
    *out = malloc(sizeof(char) * file_size);
    fseek(file, 0, SEEK_SET);

    fread(*out, sizeof(char) * file_size, 1, file);

    // Close it
    fclose(file);

    *size = file_size;
}

bool
vk_supports_validation_layers(void)
{
    // Get layers
    unsigned int layer_c;
    vkEnumerateInstanceLayerProperties(&layer_c, NULL);

    VkLayerProperties layers[layer_c];
    vkEnumerateInstanceLayerProperties(&layer_c, layers);

    // Look for needed layers
    for(unsigned int i = 0; i < layer_c; i++)
        if(strcmp(layers[i].layerName, VK_layer[0]) == 0)
            return true;

    fprintf(stderr, "Vulkan does not support validation layers!\n");

    return false;
    
}

bool
vk_create_instance(void)
{
    // Set app info
    const VkApplicationInfo pinfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = WN_NAME,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "None",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    // Set instance info
    const VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &pinfo,
        .enabledExtensionCount = VK_ext_c,
        .ppEnabledExtensionNames = VK_ext,
        .enabledLayerCount = VK_layer_c,
        .ppEnabledLayerNames = VK_layer
    };

    // Create vulkan instance
    VkResult success = vkCreateInstance(&info, NULL, &game.vk.instance);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create instance!\n");
        vk_error_print(success);

        return false;
    }

    return true;
}

bool
vk_create_window_surface(void)
{
    // Set surface info
    const VkXcbSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = game.xcb.connection,
        .window = game.xcb.window
    };

    // Create the surface
    const VkResult success = vkCreateXcbSurfaceKHR(game.vk.instance, 
                                                   &info, 
                                                   NULL, 
                                                   &game.vk.surface);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan-XCB surface!\n");
        vk_error_print(success);

        return false;
    }

    return true;
}

bool
vk_get_queue_families(
    VkPhysicalDevice device,
    unsigned int *gp_family,
    unsigned int *pr_family
    )
{
    // Get queue families
    unsigned int family_c = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_c, NULL);

    VkQueueFamilyProperties families[family_c];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_c, families);

    // Look for the graphics and present families
    bool gp_set, pr_set;
    gp_set = pr_set = false;

    for(unsigned int i = 0; i < family_c; i++)
    {
        if((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
                                                        && gp_set == false) {
            gp_set = true;
            *gp_family = i;
        }

        VkBool32 supported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, 
                                             i, 
                                             game.vk.surface, 
                                             &supported);

        if(supported) {
            pr_set = true;
            *pr_family = i;
        }

        if(pr_set && gp_set)
            return true;
    }

    return false;
}

bool
device_suitable(VkPhysicalDevice device)
{
    // Get queue families
    unsigned int gp_family, pr_family;
    if(!vk_get_queue_families(device, &gp_family, &pr_family))
        return false; 

    // Check extention support
    unsigned int ext_c;
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_c, NULL);

    VkExtensionProperties exts[ext_c];
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_c, exts);

    bool has[1] = {false};

    for(unsigned int i = 0; i < ext_c; i++)
        for(unsigned int a = 0; a < VK_dev_ext_c; a++)
            if(strcmp(exts[i].extensionName, VK_dev_ext[a]) == 0)
                has[a] = true;

    unsigned int has2 = 0;

    for(unsigned int i = 0; i < VK_dev_ext_c; i++)
        if(has[i])
            has2++;

    if(has2 != VK_dev_ext_c)
        return false;

    // Check swapchain support
    VkSurfaceCapabilitiesKHR sr_cap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, 
                                              game.vk.surface, 
                                              &sr_cap);

    // Check surface formats
    unsigned int format_c;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, 
                                         game.vk.surface, 
                                         &format_c, 
                                         NULL);

    if(format_c == 0)
        return false;

    // Check for presentation modes
    unsigned int mode_c;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, 
                                              game.vk.surface, 
                                              &mode_c, 
                                              NULL);

    if(mode_c == 0)
        return false;

    game.vk.surface_cap = sr_cap;

    // Get the format we'll use
    VkSurfaceFormatKHR formats[format_c];
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, 
                                         game.vk.surface, 
                                         &format_c, 
                                         formats);

    bool set = false;
    for(unsigned int i = 0; i < format_c; i++)
        if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && 
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

                game.vk.surface_format = formats[i];
                set = true;
                break;
            }

    if(!set)
        game.vk.surface_format = formats[0];

    // Get the presentation mode we'll use
    VkPresentModeKHR modes[mode_c];
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, 
                                              game.vk.surface, 
                                              &mode_c, 
                                              modes);

    set = false;
    for(unsigned int i = 0; i < mode_c; i++)
        if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            game.vk.surface_mode = modes[i];
            set = true;

            break;
        }

    if(!set)
        game.vk.surface_mode = VK_PRESENT_MODE_FIFO_KHR;
    
    game.vk.ex = game.vk.surface_cap.currentExtent;

    return true;
}

bool
vk_get_physical_device(void)
{
    // Get physical devices
    unsigned int device_c;
    vkEnumeratePhysicalDevices(game.vk.instance, &device_c, NULL);

    if(device_c == 0) {
        fprintf(stderr, "No physical device supports Vulkan!\n");
        return false;
    }

    VkPhysicalDevice devices[device_c];
    vkEnumeratePhysicalDevices(game.vk.instance, &device_c, devices);

    // Find the proper device
    for( unsigned int i = 0; i < device_c; i++)
        if(device_suitable(devices[i])) {
            game.vk.physical_device = devices[i];
            return true;
        }

    fprintf(stderr, "Failed to find suitable physical device!\n");
    return false;
}

bool
vk_create_logic_device(void)
{
    // Get queue families
    unsigned int gp_family, pr_family;
    vk_get_queue_families(game.vk.physical_device, &gp_family, &pr_family);

    // Set queue info
    const float priority = 1.0;
    const VkDeviceQueueCreateInfo qinfo[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = gp_family,
            .queueCount = 1,
            .pQueuePriorities = &priority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = pr_family,
            .queueCount = 1,
            .pQueuePriorities = &priority
        }
    };

    // Set device info
    const VkPhysicalDeviceFeatures dev_features = {};

    int info_count = 2;
    if(gp_family == pr_family)
        info_count = 1;

    const VkDeviceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = info_count,
        .pQueueCreateInfos = qinfo,
        .pEnabledFeatures = &dev_features,
        .enabledExtensionCount = VK_dev_ext_c,
        .ppEnabledExtensionNames = VK_dev_ext,
        .enabledLayerCount = VK_layer_c,
        .ppEnabledLayerNames = VK_layer
    };

    // Create device
    const VkResult success = vkCreateDevice(game.vk.physical_device, 
                                            &info, 
                                            NULL, 
                                            &game.vk.device);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "failed to create Vulkan Logical device!\n");
        vk_error_print(success);

        return false;
    }

    // Get the actual queue for each family
    vkGetDeviceQueue(game.vk.device, gp_family, 0, &game.vk.gp_queue);
    vkGetDeviceQueue(game.vk.device, pr_family, 0, &game.vk.pr_queue);

    return true;
}

bool
vk_recreate_swapchain()
{
    vkDeviceWaitIdle(game.vk.device);

    // Clean up old swapchain
    for(unsigned int i = 0; i < game.vk.image_c; i++)
        vkDestroyFramebuffer(game.vk.device, game.vk.framebuffers[i], NULL);

    free(game.vk.framebuffers);

    for(unsigned int i = 0; i < game.vk.image_c; i++)
        vkDestroyImageView(game.vk.device, game.vk.views[i], NULL);

    free(game.vk.views);

    vkDestroySwapchainKHR(game.vk.device, game.vk.swap, NULL);

    // Create new swapchain
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(game.vk.physical_device, 
                                              game.vk.surface, 
                                              &game.vk.surface_cap);

    game.vk.ex = game.vk.surface_cap.currentExtent;
    
    if(
        !vk_create_swapchain()   ||
        !vk_create_image_views() ||
        !vk_create_framebuffers()
    ) {
        fprintf(stderr, "Failed to recreate framebuffer!\n");
        return false;
    }

    return true;
}

bool
vk_create_swapchain(void)
{
    // Get image count
    game.vk.image_c = game.vk.surface_cap.minImageCount + 1;
    
    if(game.vk.surface_cap.maxImageCount > 0 && 
                        game.vk.image_c > game.vk.surface_cap.maxImageCount)
        game.vk.image_c = game.vk.surface_cap.maxImageCount;

    // Get sharing mode
    VkSharingMode sharing;
    unsigned int index_c, gp_family, pr_family;
    vk_get_queue_families(game.vk.physical_device, &gp_family, &pr_family);
    unsigned int families[] = {gp_family, gp_family};
    
    if(gp_family != pr_family) {
        sharing = VK_SHARING_MODE_CONCURRENT;
        index_c = 2;
    } else {
        sharing = VK_SHARING_MODE_EXCLUSIVE;
        index_c = 0;
    }

    // Set info
    const VkSwapchainCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = game.vk.surface,
        .minImageCount = game.vk.image_c,
        .imageFormat = game.vk.surface_format.format,
        .imageColorSpace = game.vk.surface_format.colorSpace,
        .imageExtent = game.vk.ex,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = sharing,
        .queueFamilyIndexCount = index_c,
        .pQueueFamilyIndices = families,
        .preTransform = game.vk.surface_cap.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = game.vk.surface_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    // Create swapchain
    VkResult success = vkCreateSwapchainKHR(game.vk.device, 
                                            &info, 
                                            NULL, 
                                            &game.vk.swap);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swap chain!\n");
        vk_error_print(success);

        return false;
    }

    // Get swapchain images

    vkGetSwapchainImagesKHR(game.vk.device, 
                            game.vk.swap, 
                            &game.vk.image_c, 
                            NULL);

    game.vk.images = malloc(sizeof(VkImage) * game.vk.image_c);

    vkGetSwapchainImagesKHR(game.vk.device, 
                            game.vk.swap, 
                            &game.vk.image_c, 
                            game.vk.images);

    return true;
}

bool
vk_create_image_views(void)
{
    // Loop throught all images
    game.vk.views = malloc(sizeof(VkImageView) * game.vk.image_c);

    for(unsigned int i = 0; i < game.vk.image_c; i++)
    {   
        // Set info
        const VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = game.vk.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = game.vk.surface_format.format,

            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },

            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        // Create image view
        VkResult success = vkCreateImageView(game.vk.device, 
                                             &info, 
                                             NULL, 
                                             &game.vk.views[i]);

        if(success != VK_SUCCESS) {
            fprintf(stderr, "Failed to create image views!\n");
            vk_error_print(success);

            return false;
        }
    }

    return true;
}

bool
vk_create_render_pass(void)
{
    // Set info
    const VkAttachmentDescription color = {
        .format = game.vk.surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    const VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref
    };

    const VkSubpassDependency subpass_dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    const VkRenderPassCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dep
    };

    // Create render pass
    VkResult success = vkCreateRenderPass(game.vk.device, 
                                          &info, 
                                          NULL, 
                                          &game.vk.render_pass);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create render pass!\n");
        vk_error_print(success);

        return false;
    }

    return true;
}

bool
vk_create_graphics_pipeline(void)
{
    // Create shaders
    VkShaderModule v_shader, f_shader;
    size_t v_size, f_size;
    char *f = NULL; 
    char *v = NULL;

    vk_read_file("shaders/vert.spv", &v_size, &v);
    vk_read_file("shaders/frag.spv", &f_size, &f);

    if(v == NULL || f == NULL)
        return false;

    const VkShaderModuleCreateInfo f_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = f_size,
        .pCode = (unsigned int *)f
    };

    VkResult success = vkCreateShaderModule(game.vk.device, 
                                            &f_shader_info, 
                                            NULL, 
                                            &f_shader);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create fragment shader!\n");
        vk_error_print(success);

        free(f);
        free(v);
        return false;
    }

    const VkShaderModuleCreateInfo v_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = v_size,
        .pCode = (unsigned int *)v
    };

    success = vkCreateShaderModule(game.vk.device,
                                   &v_shader_info,
                                   NULL,
                                   &v_shader);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vertex sahder!\n");
        vk_error_print(success);

        vkDestroyShaderModule(game.vk.device, f_shader, NULL);
        free(f);
        free(v);
        return false;
    }

    const VkPipelineShaderStageCreateInfo shader_stage[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = v_shader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = f_shader,
            .pName = "main"
        }
    };

    // Set fixed functions
    const VkPipelineVertexInputStateCreateInfo v_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkPipelineViewportStateCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    const VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    const VkPipelineMultisampleStateCreateInfo multi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    const VkPipelineColorBlendAttachmentState color_at = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                          VK_COLOR_COMPONENT_G_BIT | 
                          VK_COLOR_COMPONENT_B_BIT | 
                          VK_COLOR_COMPONENT_A_BIT,

        .blendEnable = VK_FALSE
    };

    const VkPipelineColorBlendStateCreateInfo color = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_at,
        .blendConstants = {
            0.0f,
            0.0f,
            0.0f,
            0.0f
        }
    };

    const VkDynamicState dym_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    const VkPipelineDynamicStateCreateInfo dym = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dym_states
    };

    const VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };

    // Create pipeline layout
    success = vkCreatePipelineLayout(game.vk.device, 
                                     &layout, 
                                     NULL, 
                                     &game.vk.pipeline_layout);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline layout!\n");
        vk_error_print(success);

        vkDestroyShaderModule(game.vk.device, v_shader, NULL);
        vkDestroyShaderModule(game.vk.device, f_shader, NULL);
        free(f);
        free(v);
        return false;
    }

    // Set info
    const VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stage,
        .pVertexInputState = &v_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &view,
        .pRasterizationState = &raster,
        .pMultisampleState = &multi,
        .pColorBlendState = &color,
        .pDynamicState = &dym,
        .layout = game.vk.pipeline_layout,
        .renderPass = game.vk.render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE
    };

    // Create graphics pipeline
    success = vkCreateGraphicsPipelines(game.vk.device,
                                        VK_NULL_HANDLE,
                                        1,
                                        &info,
                                        NULL, 
                                        &game.vk.pipeline);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create graphics pipeline!\n");
        vk_error_print(success);

        vkDestroyShaderModule(game.vk.device, v_shader, NULL);
        vkDestroyShaderModule(game.vk.device, f_shader, NULL);
        free(f);
        free(v);
        return false;
    }

    vkDestroyShaderModule(game.vk.device, v_shader, NULL);
    vkDestroyShaderModule(game.vk.device, f_shader, NULL);
    free(f);
    free(v);

    return true;
}

bool
vk_create_framebuffers(void)
{
    // Loop through the framebuffers
    game.vk.framebuffers = malloc(sizeof(VkFramebuffer) * game.vk.image_c);
    for(unsigned int i = 0; i < game.vk.image_c; i++)
    {
        // Set info
        const VkImageView attach[] = {
            game.vk.views[i]
        };

        const VkFramebufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = game.vk.render_pass,
            .attachmentCount = 1,
            .pAttachments = attach,
            .width = game.vk.ex.width,
            .height = game.vk.ex.height,
            .layers = 1
        };

        // Create framebuffer
        VkResult success = vkCreateFramebuffer(game.vk.device, 
                                               &info, 
                                               NULL, 
                                               &game.vk.framebuffers[i]);

        if(success != VK_SUCCESS) {
            fprintf(stderr, "Failed to create framebuffer!\n");
            vk_error_print(success);

            return false;
        }
    }

    return true;
}

bool
vk_create_cmd_pool(void)
{
    // Get queue families and set info
    unsigned int gp_family, pr_family;
    vk_get_queue_families(game.vk.physical_device, &gp_family, &pr_family);

    const VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = gp_family
    };

    // Create the command pool
    VkResult success = vkCreateCommandPool(game.vk.device, 
                                           &info, 
                                           NULL, 
                                           &game.vk.cmdpool);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool!\n");
        vk_error_print(success);

        return false;
    }

    return true;
}

bool
vk_create_cmd_buffer(void)
{
    game.vk.cmdbuffer = malloc(sizeof(VkCommandBuffer) * game.vk.max_frames);

    // Set info
    const VkCommandBufferAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = game.vk.cmdpool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = game.vk.max_frames
    };

    // Create command buffer
    VkResult success = vkAllocateCommandBuffers(game.vk.device, 
                                                &info, 
                                                game.vk.cmdbuffer);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command buffer!\n");
        vk_error_print(success);

        return false;
    }

    return true;
}

bool
vk_create_sync_objects(void)
{
    game.vk.img_available = malloc(sizeof(VkSemaphore) * game.vk.max_frames);
    game.vk.render_finished = malloc(sizeof(VkSemaphore) * game.vk.max_frames);
    game.vk.flight = malloc(sizeof(VkFence) * game.vk.max_frames);

    // Set info
    const VkSemaphoreCreateInfo info_s = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    const VkFenceCreateInfo info_f = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    // Create semaphores
    for(unsigned int i = 0; i < game.vk.max_frames; i++)
    {
        VkResult success = vkCreateSemaphore(game.vk.device, 
                                            &info_s, 
                                            NULL, 
                                            &game.vk.img_available[i]);
        
        if(success != VK_SUCCESS) {
            fprintf(stderr, "Failed to create semaphore!\n");
            vk_error_print(success);

            return false;
        }

        success = vkCreateSemaphore(game.vk.device, 
                                    &info_s, 
                                    NULL, 
                                    &game.vk.render_finished[i]);

        if(success != VK_SUCCESS) {
            fprintf(stderr, "Failed to create semaphore!\n");
            vk_error_print(success);

            return false;
        }

        // Create fence
        success = vkCreateFence(game.vk.device, &info_f, NULL, &game.vk.flight[i]);

        if(success != VK_SUCCESS) {
            fprintf(stderr, "Failed to create fence!\n");
            vk_error_print(success);
            
            return false;
        }
    }

    return true;
}