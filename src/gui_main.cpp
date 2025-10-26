#include "paa.h"
#include "image_loader.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <portable-file-dialogs.h>

#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

struct ConversionJob {
    std::string inputPath;
    std::string outputPath;
    bool completed = false;
    bool success = false;
    std::string errorMessage;
    int64_t durationMs = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

class PAAConverterApp {
public:
    PAAConverterApp() : selectedFormat(0), isConverting(false) {
        formatNames[0] = "Auto (DXT1/DXT5)";
        formatNames[1] = "DXT1 (No Alpha)";
        formatNames[2] = "DXT5 (With Alpha)";
    }

    void render() {
        // Make window fill the entire viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGui::Begin("Arma 3 PAA Converter", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Title section
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Native C++ PAA Converter");
        ImGui::Separator();

        // File input section
        ImGui::Text("Input Files:");
        ImGui::SameLine();
        ImGui::TextDisabled("(Drag & drop files here)");

        ImGui::InputTextMultiline("##files", fileListBuffer, sizeof(fileListBuffer),
            ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);

        if (ImGui::Button("Add Files...")) {
            openFileDialog();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            fileListBuffer[0] = '\0';
            inputFiles.clear();
        }

        // Format selection
        ImGui::Spacing();
        ImGui::Text("Output Format:");
        ImGui::Combo("##format", &selectedFormat, formatNames, 3);

        // Output directory
        ImGui::Spacing();
        ImGui::Text("Output Directory:");
        ImGui::InputText("##outdir", outputDir, sizeof(outputDir));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            openFolderDialog();
        }
        ImGui::SameLine();
        if (ImGui::Button("Same as Input")) {
            outputDir[0] = '\0';
        }

        ImGui::Separator();

        // Convert button
        ImGui::Spacing();
        bool canConvert = !isConverting && !inputFiles.empty();
        if (!canConvert) ImGui::BeginDisabled();

        if (ImGui::Button("Convert", ImVec2(120, 40))) {
            startConversion();
        }

        if (!canConvert) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::Text("Files: %zu", inputFiles.size());

        // Progress section
        if (isConverting || !conversionJobs.empty()) {
            ImGui::Separator();
            ImGui::Text("Progress:");

            if (isConverting) {
                ImGui::ProgressBar(currentProgress, ImVec2(-1, 0));
                ImGui::Text("Converting %d/%zu files...", completedJobs + 1, conversionJobs.size());
            } else {
                ImGui::Text("Conversion complete!");
                ImGui::Text("Successful: %d | Failed: %d", successCount, failCount);
            }

            // Results table
            ImGui::BeginChild("Results", ImVec2(0, 200), true);
            ImGui::Columns(4, "results");
            ImGui::Separator();
            ImGui::Text("File"); ImGui::NextColumn();
            ImGui::Text("Size"); ImGui::NextColumn();
            ImGui::Text("Time"); ImGui::NextColumn();
            ImGui::Text("Status"); ImGui::NextColumn();
            ImGui::Separator();

            for (const auto& job : conversionJobs) {
                if (!job.completed) continue;

                fs::path p(job.inputPath);
                ImGui::Text("%s", p.filename().string().c_str()); ImGui::NextColumn();
                ImGui::Text("%dx%d", job.width, job.height); ImGui::NextColumn();
                ImGui::Text("%lldms", job.durationMs); ImGui::NextColumn();

                if (job.success) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Success");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", job.errorMessage.c_str());
                    }
                }
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
            ImGui::EndChild();

            if (!isConverting && ImGui::Button("Clear Results")) {
                conversionJobs.clear();
                successCount = 0;
                failCount = 0;
            }
        }

        // Stats section
        ImGui::Separator();
        ImGui::TextDisabled("Native C++ with libsquish DXT compression");
        ImGui::TextDisabled("50-100x faster than WASM");

        ImGui::End();
    }

    void handleFileDrop(const std::vector<std::string>& files) {
        for (const auto& file : files) {
            std::string ext = fs::path(file).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg") {
                inputFiles.push_back(file);
            }
        }

        updateFileListBuffer();
    }

    void openFileDialog() {
        // Create file dialog for selecting images
        auto selection = pfd::open_file("Select Image Files",
            "",
            { "Image Files", "*.png *.tga *.jpg *.jpeg",
              "PNG Files", "*.png",
              "TGA Files", "*.tga",
              "JPEG Files", "*.jpg *.jpeg",
              "All Files", "*" },
            pfd::opt::multiselect);

        auto files = selection.result();
        if (!files.empty()) {
            for (const auto& file : files) {
                inputFiles.push_back(file);
            }
            updateFileListBuffer();
        }
    }

    void openFolderDialog() {
        // Create folder dialog for selecting output directory
        auto selection = pfd::select_folder("Select Output Directory", "");
        std::string folder = selection.result();

        if (!folder.empty()) {
            strncpy(outputDir, folder.c_str(), sizeof(outputDir) - 1);
            outputDir[sizeof(outputDir) - 1] = '\0';
        }
    }

private:
    void updateFileListBuffer() {
        std::string text;
        for (const auto& file : inputFiles) {
            text += file + "\n";
        }
        strncpy(fileListBuffer, text.c_str(), sizeof(fileListBuffer) - 1);
    }

    void startConversion() {
        conversionJobs.clear();
        isConverting = true;
        completedJobs = 0;
        successCount = 0;
        failCount = 0;

        for (const auto& input : inputFiles) {
            ConversionJob job;
            job.inputPath = input;

            std::string outDir = outputDir[0] != '\0' ? outputDir : fs::path(input).parent_path().string();
            job.outputPath = (fs::path(outDir) / (fs::path(input).stem().string() + ".paa")).string();

            conversionJobs.push_back(job);
        }

        // Start conversion in background thread
        std::thread([this]() {
            for (size_t i = 0; i < conversionJobs.size(); i++) {
                auto& job = conversionJobs[i];

                try {
                    auto start = std::chrono::high_resolution_clock::now();

                    arma3::PAA paa;
                    paa.loadImage(job.inputPath);

                    arma3::PAAFormat format = arma3::PAAFormat::UNKNOWN;
                    if (selectedFormat == 1) format = arma3::PAAFormat::DXT1;
                    else if (selectedFormat == 2) format = arma3::PAAFormat::DXT5;

                    job.width = paa.getMipMaps()[0].width;
                    job.height = paa.getMipMaps()[0].height;

                    paa.writePAA(job.outputPath, format);

                    auto end = std::chrono::high_resolution_clock::now();
                    job.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                    job.success = true;
                    successCount++;
                }
                catch (const std::exception& e) {
                    job.success = false;
                    job.errorMessage = e.what();
                    failCount++;
                }

                job.completed = true;
                completedJobs++;
                currentProgress = static_cast<float>(completedJobs) / conversionJobs.size();
            }

            isConverting = false;
        }).detach();
    }

    char fileListBuffer[4096] = {0};
    char outputDir[256] = {0};
    int selectedFormat;
    const char* formatNames[3];
    std::vector<std::string> inputFiles;

    bool isConverting;
    int completedJobs = 0;
    int successCount = 0;
    int failCount = 0;
    float currentProgress = 0.0f;
    std::vector<ConversionJob> conversionJobs;
};

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths) {
    PAAConverterApp* app = static_cast<PAAConverterApp*>(glfwGetWindowUserPointer(window));
    std::vector<std::string> files;
    for (int i = 0; i < count; i++) {
        files.push_back(paths[i]);
    }
    app->handleFileDrop(files);
}

int main(int argc, char** argv) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        return 1;
    }

    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 700, "Arma 3 PAA Converter", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Create app
    PAAConverterApp app;
    glfwSetWindowUserPointer(window, &app);
    glfwSetDropCallback(window, glfw_drop_callback);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render app
        app.render();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
