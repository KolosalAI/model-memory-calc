#ifndef MODEL_FILE_H
#define MODEL_FILE_H

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "gguf_reader.h"

#ifdef __EMSCRIPTEN__
  #include <emscripten/bind.h>
#endif

/**
 * @brief Information about quantization type and quality
 */
struct QuantizationInfo {
    std::string type;         ///< Quantization type (e.g., "Q8_0", "Q4_K_M")
    std::string description;  ///< Human-readable description
    int priority = 9999;      ///< Priority for default selection (lower = higher priority)
};

/**
 * @brief Memory usage estimation for a model
 */
struct MemoryUsage {
    size_t modelSizeMB = 0;       ///< Model size in MB (decimal MB: 1e6 bytes)
    size_t kvCacheMB = 0;         ///< KV cache size in MB (decimal)
    size_t totalRequiredMB = 0;   ///< Total required memory in MB (decimal)
    std::string displayString;    ///< Formatted display string
    bool hasEstimate = false;     ///< Whether we have valid estimates
    bool isLoading = false;       ///< Whether memory calculation is in progress

#ifndef __EMSCRIPTEN__
    // In browsers (no pthreads by default), we do sync calc. Keep the future only for native.
    std::shared_ptr<std::future<MemoryUsage>> asyncResult;
#endif
};

/**
 * @brief Represents a model file with quantization information
 */
struct ModelFile {
    std::string filename;                 ///< File name (e.g., foo.Q4_K_M.gguf)
    std::string modelId;                  ///< Full model ID (e.g., "kolosal/model-name")
    QuantizationInfo quant;               ///< Quantization info
    std::optional<std::string> downloadUrl; ///< URL (if any)
    MemoryUsage memoryUsage;              ///< Memory usage estimation

    std::string getDisplayName() const;
    std::string getDisplayNameWithMemory() const;
    bool updateDisplayIfReady();
};

/**
 * @brief Utility class for model file operations
 */
class ModelFileUtils {
public:
    static QuantizationInfo detectQuantization(const std::string& filename);
    static void sortByPriority(std::vector<ModelFile>& modelFiles);

    /**
     * @brief Calculate memory usage estimation for a model file
     *        (sync; safe for WASM)
     */
    static MemoryUsage calculateMemoryUsage(const ModelFile& modelFile, int contextSize = 4096);

#ifndef __EMSCRIPTEN__
    /**
     * @brief Start async memory usage calculation (native only by default)
     */
    static MemoryUsage calculateMemoryUsageAsync(const ModelFile& modelFile, int contextSize = 4096);

    /**
     * @brief Update memory usage if async calculation is complete (native)
     */
    static bool updateAsyncMemoryUsage(MemoryUsage& memoryUsage);

    /**
     * @brief Update memory usage for all model files (native)
     */
    static bool updateAllAsyncMemoryUsage(std::vector<ModelFile>& modelFiles);
#else
    // In WASM we keep the same signatures available but implement them as sync fallbacks.
    static MemoryUsage calculateMemoryUsageAsync(const ModelFile& modelFile, int contextSize = 4096) {
        // For browsers (no pthreads by default), do it synchronously.
        MemoryUsage u = calculateMemoryUsage(modelFile, contextSize);
        u.isLoading = false;
        return u;
    }
    static bool updateAsyncMemoryUsage(MemoryUsage&) { return false; }
    static bool updateAllAsyncMemoryUsage(std::vector<ModelFile>&) { return false; }
#endif

    static size_t estimateModelSize(const GGUFModelParams& params, const std::string& quantType);
    static std::string formatMemorySize(size_t sizeInMB);

    /**
     * @brief Get actual file size from URL using HTTP HEAD
     * @return File size in bytes, or 0 if unknown
     */
    static size_t getActualFileSizeFromUrl(const std::string& url);

    // The interactive / cache utilities are omitted for WASM (terminal/extern deps).
};

#ifdef __EMSCRIPTEN__
// Embind helpers so JS can call directly.
emscripten::val calcMemoryFromUrl(const std::string& modelId,
                                  const std::string& filename,
                                  const std::string& url,
                                  int contextSize);

emscripten::val calcMemoryFromFile(const std::string& modelId,
                                   const std::string& filename,
                                   const std::string& path,
                                   int contextSize);
#endif

#endif // MODEL_FILE_H
