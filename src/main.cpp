#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
    "VK_LAYER_KHRONOS_validation",
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

    vk::raii::Context                context;
    vk::raii::Instance               instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::SurfaceKHR             surface = nullptr;

    vk::raii::PhysicalDevice         physicalDevice = nullptr;
    vk::raii::Device                 device = nullptr;
    vk::raii::Queue                  graphicsQueue = nullptr;

    vk::raii::SwapchainKHR           swapChain = nullptr;
    std::vector<vk::Image>           swapChainImages;
	vk::Extent2D                     swapChainExtent;
    vk::SurfaceFormatKHR             swapChainSurfaceFormat;
    std::vector<vk::raii::ImageView> swapChainImageViews;
    
    vk::raii::PipelineLayout         pipelineLayout = nullptr;
    vk::raii::Pipeline               graphicsPipeline = nullptr;

    // Used to determine if a GPU has our required extensions
    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName,
    };

    //* Conductor functions
    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Disable resizing for now

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
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

    //* Init functions
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

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        // These are the severity/messages flags the debugCallback will listen for
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
                                                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | 
                                                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT {
            .messageSeverity = severityFlags,
            .messageType     = messageTypeFlags,
            .pfnUserCallback = &debugCallback,
        };

        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void createSurface() {
        VkSurfaceKHR _surface; // This is a C API object so we promoted it to a C++ wrapper instead of using it directly
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    void pickPhysicalDevice() {
        auto physicalDevices = instance.enumeratePhysicalDevices();

        auto const devIter = std::ranges::find_if(
            physicalDevices,
            [&](auto const &physicalDevice) {
                return isDeviceSuitable(physicalDevice);
            });
        
        if (devIter == physicalDevices.end()) {
            throw std::runtime_error( "failed to find a suitable GPU!" );
        }
        
        physicalDevice = *devIter;
    }

    void createLogicalDevice() {
        // Find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        uint32_t queueIndex = ~0u; // UINT32_MAX
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); ++qfpIndex) {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
                // found a queue family that supports both graphics and present
                queueIndex = qfpIndex;
                break;
            }
        }

        if (queueIndex == ~0u) {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo {
            .queueFamilyIndex = queueIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
        
        // Currently, we don't need any more deviceFeatures so we'll just define it and leave it. We'll come back to it later.
        vk::PhysicalDeviceFeatures deviceFeatures;

        // Create a chain of feature structures
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                        vk::PhysicalDeviceVulkan11Features,
                        vk::PhysicalDeviceVulkan13Features,
                        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},                             // vk::PhysicalDeviceFeatures2 (empty for now)
                {.shaderDrawParameters = true}, // Enable shader draw parameters from Vulkan 1.1
                {.dynamicRendering = true},     // Enable dynamic rendering from Vulkan 1.3
                {.extendedDynamicState = true}, // Enable extended dynamic state from the extension
            };

        vk::DeviceCreateInfo deviceCreateInfo {
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
    }

    void createSwapChain() {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( *surface );
        swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
        
        std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo {
            .surface          = *surface,
            .minImageCount    = minImageCount,
            .imageFormat      = swapChainSurfaceFormat.format,
            .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
            .imageExtent      = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform     = surfaceCapabilities.currentTransform,
            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode      = presentMode,
            .clipped          = true,
        };

        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();
    }

    void createImageViews() {
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo {
            .viewType         = vk::ImageViewType::e2D,
            .format           = swapChainSurfaceFormat.format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        };

        for (auto &image : swapChainImages) {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    void createGraphicsPipeline() {
        static const std::string SHADER_DIR = "build/VulkanEngine/shaders/slang.spv";

        // We don't need to keep the buffer after creating the shader module nor
        // the shader module after the pipeline is created
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile(SHADER_DIR));

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = "vertMain",
        };
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain",
        };
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // The following configuration is for fixed functions in the graphics pipeline

        // Leave empty for now
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

        vk::PipelineRasterizationStateCreateInfo rasterizer {
            .depthClampEnable        = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eBack,
            .frontFace               = vk::FrontFace::eClockwise,
            .depthBiasEnable         = vk::False,
            .lineWidth               = 1.0f,
        };

        // We'll revist this later
        vk::PipelineMultisampleStateCreateInfo multisampling {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False,
        };

        vk::PipelineColorBlendAttachmentState colorBlendAttachment {
		    .blendEnable    = vk::False,
		    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | 
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

		vk::PipelineColorBlendStateCreateInfo colorBlending {
		    .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
        };

		std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState {
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        // Used to specify the values of uniform values in shaders (not used yet)
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};
        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
           {.stageCount          = 2,
            .pStages             = shaderStages,
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = &dynamicState,
            .layout              = pipelineLayout,
            .renderPass          = nullptr},
            {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format}
        };

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }

    //* Helper functions
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

    bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) {
        // Check if the physicalDevice supports the Vulkan 1.3 API version
        bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

        // Check if any of the queue families support graphics operations
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of(
            queueFamilies,
            [](auto const &qfp) {
                return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
            });

        // Check if all required physicalDevice extensions are available
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = std::ranges::all_of(
            requiredDeviceExtension,
            [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
                return std::ranges::any_of(
                    availableDeviceExtensions,
                    [requiredDeviceExtension](auto const &availableDeviceExtension) {
                        return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                    });
            });

        // Check if the physicalDevice supports the required features:
        //   shader draw parameters, dynamic rendering, and extended dynamic state
        auto features = physicalDevice.template getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = 
            features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        // Return true if the physicalDevice meets all the criteria
        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }
    
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) {
        // currentExtent is only set to the special "undefined" value described above
        // when the window manager lets us choose the extent ourselves; any other value
        // means the surface already dictates a fixed extent that we must use as-is.
        if (capabilities.currentExtent.width != ~0u) {
            return capabilities.currentExtent;
        }
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }
    
    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities) {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount); // By default, we will try to choose 3 images
        if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        const auto formatIt = std::ranges::find_if(
            availableFormats,
            [](const auto &format) {
                return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });
        return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes) {
        assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(
            availablePresentModes,
            [](const vk::PresentModeKHR value) { 
                return vk::PresentModeKHR::eMailbox == value; 
            }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    [[nodiscard]] 
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
        vk::ShaderModuleCreateInfo createInfo {
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };

        vk::raii::ShaderModule shaderModule{device, createInfo};
        return shaderModule;
    }

    
    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        // We start reading at the end (ios::ate) and use tellg to determine the size of the file
        std::vector<char> buffer(file.tellg());

        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();

        return buffer;
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
