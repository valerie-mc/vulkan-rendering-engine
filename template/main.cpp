// #include <algorithm>
#include <cstdlib>
// #include <cstring>
#include <iostream>
// #include <memory>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


// Validation Layers
const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

// App Settings
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class HelloTriangleApplication {
  public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    GLFWwindow *window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Disable resizing for now

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
    }

    void pickPhysicalDevice() {
        auto physicalDevices = instance.enumeratePhysicalDevices();

        if (physicalDevices.empty()) {
            throw std::runtime_error("Failed to find a GPU with Vulkan support.");
        }

        for (auto physicalDevice : physicalDevices) {
            break;
        }
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {
        // Get the required layers.
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }
        checkValidationLayersSupported(requiredLayers);
        
        auto requiredExtensions = getRequiredInstanceExtensions();
        checkExtensionsSupported(requiredExtensions);

        constexpr vk::ApplicationInfo appInfo {
            .pApplicationName   = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
            .apiVersion         = vk::ApiVersion14,
        };

        
        vk::InstanceCreateInfo createInfo {
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames     = requiredLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data(),
        };

        instance = vk::raii::Instance(context, createInfo);
    }

    // Checks if the required layers are supported by the Vulkan implementation.
    // Throws an error if they are not.
    void checkValidationLayersSupported(std::vector<const char*> requiredLayers) {
        auto layers = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(
            requiredLayers,
            [&layers](auto const &requiredLayer) {
                return std::ranges::none_of(
                    layers,
                    [requiredLayer](auto const &layer) {
                        return strcmp(layer.layerName, requiredLayer) == 0; 
                    });
            });
        
        if (unsupportedLayerIt != requiredLayers.end()) {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }
    }

    // Returns a vector of pointers to strings of the required instance extensions.
    std::vector<const char*> getRequiredInstanceExtensions() {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Essentially converts the glfwExtensions: char** to a vector<const char*>
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    // Checks if the required extensions are supported by the Vulkan implementation.
    // Throws an error if they are not.
    void checkExtensionsSupported(std::vector<const char*> requiredExtensions) {
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt =
            std::ranges::find_if(
                requiredExtensions,
                [&extensionProperties](auto const &requiredExtension) {
                    return std::ranges::none_of(extensionProperties,
                    [requiredExtension](auto const &extensionProperty) {
                        return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                    });
                });
        
        if (unsupportedPropertyIt != requiredExtensions.end()) {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
        }
    }


    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        // These are the severity/messages flags the debugCallback will listen for
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
                                                               vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | 
                                                               vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT {
            .messageSeverity = severityFlags,
            .messageType     = messageTypeFlags,
            .pfnUserCallback = &debugCallback
        };

        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    // Debug callback fn for validation layers.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
        vk::DebugUtilsMessageTypeFlagsEXT              type,
        const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
        void *                                         pUserData) 
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
        }

        // Currently not used, see `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html#_message_callback` for more info.
        // You need to modify `this.setupDebugMessenger` to be called for these flags if you want to use them.
        // if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose ||
        //     severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo);

        return vk::False;
    }
};

int main() {
    try {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
