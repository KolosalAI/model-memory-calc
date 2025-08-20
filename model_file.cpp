#include "model_file.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <iostream>

#ifndef __EMSCRIPTEN__
  #include <future>
  #include <chrono>
  #include <thread>
  #include <curl/curl.h>
#else
  #include <emscripten.h>
  #include <emscripten/bind.h>
#endif

// ---------- ModelFile display helpers ----------
std::string ModelFile::getDisplayName() const {
    std::string modelName = modelId;
    size_t lastSlash = modelName.find_last_of('/');
    if (lastSlash != std::string::npos) modelName = modelName.substr(lastSlash + 1);
    std::transform(modelName.begin(), modelName.end(), modelName.begin(),
                   [](char c){ return (c == '_') ? '-' : (char)std::tolower(c); });
    return modelName + ":" + quant.type;
}

std::string ModelFile::getDisplayNameWithMemory() const {
    std::string base = getDisplayName();
    if (memoryUsage.isLoading) return base + " [Memory: calculating...]";
    if (memoryUsage.hasEstimate) return base + " [Memory: " + memoryUsage.displayString + "]";
    return base;
}

bool ModelFile::updateDisplayIfReady() {
#ifndef __EMSCRIPTEN__
    return ModelFileUtils::updateAsyncMemoryUsage(memoryUsage);
#else
    (void)0; // no-op in WASM (sync path)
    return false;
#endif
}

// ---------- Quantization detection ----------
QuantizationInfo ModelFileUtils::detectQuantization(const std::string& filename) {
    std::string s = filename;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    auto pick = [](std::string t, std::string d, int p) {
        return QuantizationInfo{std::move(t), std::move(d), p};
    };

    // (Keep your original ordering/labels)
    if (s.find("iq1_s") != std::string::npos && s.find("ud-") != std::string::npos) return pick("UD-IQ1_S","1-bit UD, ultra compact",1);
    if (s.find("iq1_m") != std::string::npos && s.find("ud-") != std::string::npos) return pick("UD-IQ1_M","1-bit UD, medium variant",2);
    if (s.find("iq2_xxs")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-IQ2_XXS","2-bit UD, ultra small",3);
    if (s.find("iq2_m")  !=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-IQ2_M","2-bit UD, balanced",4);
    if (s.find("iq3_xxs")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-IQ3_XXS","3-bit UD, very small",5);
    if (s.find("q2_k_xl")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-Q2_K_XL","2-bit UD K-quant, very compact",6);
    if (s.find("q3_k_xl")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-Q3_K_XL","3-bit UD K-quant, compact",7);
    if (s.find("q4_k_xl")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-Q4_K_XL","4-bit UD K-quant, good quality",8);
    if (s.find("q5_k_xl")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-Q5_K_XL","5-bit UD K-quant, high quality",9);
    if (s.find("q6_k_xl")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-Q6_K_XL","6-bit UD K-quant, very high quality",10);
    if (s.find("q8_k_xl")!=std::string::npos && s.find("ud-")!=std::string::npos) return pick("UD-Q8_K_XL","8-bit UD K-quant, maximum quality",11);

    if (s.find("q8_k_xl")!=std::string::npos) return pick("Q8_K_XL","8-bit K-quant, maximum quality",12);
    if (s.find("q6_k_xl")!=std::string::npos) return pick("Q6_K_XL","6-bit K-quant, very high quality",13);
    if (s.find("q5_k_xl")!=std::string::npos) return pick("Q5_K_XL","5-bit K-quant, high quality",14);
    if (s.find("q4_k_xl")!=std::string::npos) return pick("Q4_K_XL","4-bit K-quant, good quality",15);
    if (s.find("q3_k_xl")!=std::string::npos) return pick("Q3_K_XL","3-bit K-quant, compact",16);
    if (s.find("q2_k_xl")!=std::string::npos) return pick("Q2_K_XL","2-bit K-quant, very compact",17);

    if (s.find("q8_0")   !=std::string::npos) return pick("Q8_0","8-bit quant, excellent quality",18);
    if (s.find("q6_k")   !=std::string::npos) return pick("Q6_K","6-bit quant, high quality",19);
    if (s.find("q5_k_m") !=std::string::npos) return pick("Q5_K_M","5-bit quant medium, balanced",20);
    if (s.find("q5_k_s") !=std::string::npos) return pick("Q5_K_S","5-bit quant small, compact",21);
    if (s.find("q5_0")   !=std::string::npos) return pick("Q5_0","5-bit quant, legacy",22);

    if (s.find("iq4_nl") !=std::string::npos) return pick("IQ4_NL","4-bit improved, very efficient",23);
    if (s.find("iq4_xs") !=std::string::npos) return pick("IQ4_XS","4-bit improved, ultra compact",24);
    if (s.find("q4_k_m") !=std::string::npos) return pick("Q4_K_M","4-bit quant medium, recommended",25);
    if (s.find("q4_k_l") !=std::string::npos) return pick("Q4_K_L","4-bit quant large, better quality",26);
    if (s.find("q4_k_s") !=std::string::npos) return pick("Q4_K_S","4-bit quant small, very compact",27);
    if (s.find("q4_1")   !=std::string::npos) return pick("Q4_1","4-bit quant v1, improved legacy",28);
    if (s.find("q4_0")   !=std::string::npos) return pick("Q4_0","4-bit quant, legacy",29);

    if (s.find("iq3_xxs")!=std::string::npos) return pick("IQ3_XXS","3-bit improved, maximum compression",30);
    if (s.find("q3_k_l") !=std::string::npos) return pick("Q3_K_L","3-bit quant large, experimental",31);
    if (s.find("q3_k_m") !=std::string::npos) return pick("Q3_K_M","3-bit quant medium, very small",32);
    if (s.find("q3_k_s") !=std::string::npos) return pick("Q3_K_S","3-bit quant small, ultra compact",33);

    if (s.find("iq2_xxs")!=std::string::npos) return pick("IQ2_XXS","2-bit improved, extreme compression",34);
    if (s.find("iq2_m")  !=std::string::npos) return pick("IQ2_M","2-bit improved, balanced",35);
    if (s.find("q2_k_l") !=std::string::npos) return pick("Q2_K_L","2-bit quant large, better quality",36);
    if (s.find("q2_k")   !=std::string::npos) return pick("Q2_K","2-bit quant, extremely small",37);

    if (s.find("iq1_s")  !=std::string::npos) return pick("IQ1_S","1-bit improved, experimental",38);
    if (s.find("iq1_m")  !=std::string::npos) return pick("IQ1_M","1-bit improved medium, experimental",39);

    if (s.find("f16")    !=std::string::npos) return pick("F16","16-bit float, highest quality",40);
    if (s.find("f32")    !=std::string::npos) return pick("F32","32-bit float, original precision",41);

    return pick("Unknown","Unknown quantization type",42);
}

void ModelFileUtils::sortByPriority(std::vector<ModelFile>& modelFiles) {
    std::sort(modelFiles.begin(), modelFiles.end(),
        [](const ModelFile& a, const ModelFile& b){ return a.quant.priority < b.quant.priority; });
}

// ---------- Memory calculation ----------
static size_t toMB_decimal(size_t bytes) { return bytes / (1000ull * 1000ull); }

MemoryUsage ModelFileUtils::calculateMemoryUsage(const ModelFile& modelFile, int contextSize) {
    MemoryUsage usage;

    // Need a URL or a local file path (when compiled with FS)
    if (!modelFile.downloadUrl.has_value() && modelFile.filename.empty()) {
        return usage;
    }

    try {
        size_t fileBytes = 0;

        if (modelFile.downloadUrl.has_value()) {
            fileBytes = getActualFileSizeFromUrl(modelFile.downloadUrl.value());
        } else {
            // Local file (available in MEMFS if you wrote it)
            std::ifstream f(modelFile.filename, std::ios::binary | std::ios::ate);
            if (f) fileBytes = static_cast<size_t>(f.tellg());
        }

        if (fileBytes == 0) {
            // Fall back to an estimate from GGUF header if we can’t HEAD the file
            GGUFMetadataReader reader;
            auto params = modelFile.downloadUrl.has_value()
                        ? reader.readModelParams(modelFile.downloadUrl.value(), false)
                        : reader.readModelParams(modelFile.filename, false);
            if (params) {
                usage.modelSizeMB = estimateModelSize(*params, modelFile.quant.type);
            } else {
                return usage; // No estimate available
            }
        } else {
            usage.modelSizeMB = toMB_decimal(fileBytes);
        }

        // Read GGUF metadata for KV calc
        GGUFMetadataReader reader;
        auto params = modelFile.downloadUrl.has_value()
                    ? reader.readModelParams(modelFile.downloadUrl.value(), false)
                    : reader.readModelParams(modelFile.filename, false);

        if (!params.has_value()) {
            return usage; // cannot compute KV
        }

        // KV cache ~ 4 * hidden_size * hidden_layers * context_size bytes
        double kvBytes = 4.0 *
                         static_cast<double>(params->hidden_size) *
                         static_cast<double>(params->hidden_layers) *
                         static_cast<double>(contextSize);

        usage.kvCacheMB = static_cast<size_t>(kvBytes / 1'000'000.0);

        // Total (you had +20% in comments but added “just sum”; keep sum as you did)
        usage.totalRequiredMB = usage.modelSizeMB + usage.kvCacheMB;

        std::ostringstream oss;
        oss << formatMemorySize(usage.totalRequiredMB)
            << " (Model: " << formatMemorySize(usage.modelSizeMB)
            << " + KV: " << formatMemorySize(usage.kvCacheMB) << ")";
        usage.displayString = oss.str();
        usage.hasEstimate = true;
        usage.isLoading = false;
        return usage;
    } catch (...) {
        return usage;
    }
}

#ifndef __EMSCRIPTEN__
// ---------- Native async helpers ----------
MemoryUsage ModelFileUtils::calculateMemoryUsageAsync(const ModelFile& modelFile, int contextSize) {
    MemoryUsage usage;
    usage.isLoading = true;
    usage.hasEstimate = false;

    auto fut = std::make_shared<std::future<MemoryUsage>>(
        std::async(std::launch::async, [modelFile, contextSize](){
            return calculateMemoryUsage(modelFile, contextSize);
        })
    );
    usage.asyncResult = fut;
    return usage;
}

bool ModelFileUtils::updateAsyncMemoryUsage(MemoryUsage& mu) {
    if (!mu.isLoading || !mu.asyncResult) return false;
    if (mu.asyncResult->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            MemoryUsage r = mu.asyncResult->get();
            mu = r; // copy result over (includes hasEstimate/displayString/etc.)
            return true;
        } catch (...) {
            mu.isLoading = false;
            mu.hasEstimate = false;
            mu.asyncResult.reset();
            return true;
        }
    }
    return false;
}

bool ModelFileUtils::updateAllAsyncMemoryUsage(std::vector<ModelFile>& modelFiles) {
    bool any = false;
    for (auto& mf : modelFiles) any |= updateAsyncMemoryUsage(mf.memoryUsage);
    return any;
}
#endif

// ---------- Model size estimate from params + quant ----------
size_t ModelFileUtils::estimateModelSize(const GGUFModelParams& params,
                                         const std::string& quantType) {
    // Very rough estimate based on your mapping
    uint64_t approx_params =
        static_cast<uint64_t>(params.hidden_size) *
        static_cast<uint64_t>(params.hidden_layers) *
        static_cast<uint64_t>(params.attention_heads) * 1000ull;

    static const std::unordered_map<std::string, float> quantBits = {
        {"F32",32.0f},{"F16",16.0f},{"Q8_0",8.5f},{"Q8_K_XL",8.5f},
        {"Q6_K",6.5f},{"Q6_K_XL",6.5f},{"Q5_K_M",5.5f},{"Q5_K_S",5.1f},
        {"Q5_K_XL",5.5f},{"Q5_0",5.5f},{"Q4_K_M",4.5f},{"Q4_K_L",4.6f},
        {"Q4_K_S",4.1f},{"Q4_K_XL",4.5f},{"Q4_0",4.5f},{"Q4_1",4.5f},
        {"IQ4_NL",4.2f},{"IQ4_XS",4.0f},{"Q3_K_L",3.4f},{"Q3_K_M",3.3f},
        {"Q3_K_S",3.2f},{"Q3_K_XL",3.4f},{"IQ3_XXS",3.1f},{"Q2_K",2.6f},
        {"Q2_K_L",2.8f},{"Q2_K_XL",2.6f},{"IQ2_XXS",2.1f},{"IQ2_M",2.4f},
        {"IQ1_S",1.6f},{"IQ1_M",1.8f},
        {"UD-Q8_K_XL",8.5f},{"UD-Q6_K_XL",6.5f},{"UD-Q5_K_XL",5.5f},
        {"UD-Q4_K_XL",4.5f},{"UD-Q3_K_XL",3.4f},{"UD-Q2_K_XL",2.6f},
        {"UD-IQ3_XXS",3.1f},{"UD-IQ2_XXS",2.1f},{"UD-IQ2_M",2.4f},
        {"UD-IQ1_S",1.6f},{"UD-IQ1_M",1.8f}
    };

    float bpp = 16.0f;
    if (auto it = quantBits.find(quantType); it != quantBits.end()) bpp = it->second;

    long double bytes = static_cast<long double>(approx_params) * (bpp / 8.0L);
    return static_cast<size_t>(bytes / 1'000'000.0L); // decimal MB
}

// ---------- Formatting ----------
std::string ModelFileUtils::formatMemorySize(size_t mb) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    if (mb >= 1000) oss << (mb / 1000.0) << " GB";
    else oss << mb << " MB";
    return oss.str();
}

// ---------- HTTP HEAD: getActualFileSizeFromUrl ----------
#ifndef __EMSCRIPTEN__
static size_t curl_head_size(const std::string& url) {
    size_t out = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_off_t len = -1;
        if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &len) == CURLE_OK) {
            if (len > 0) out = static_cast<size_t>(len);
        }
    }
    curl_easy_cleanup(curl);
    return out;
}
#else
// Async JS: HEAD and read Content-Length (or fall back to GET+arrayBuffer length if needed)
EM_ASYNC_JS(int, wasm_head_size, (const char* url), {
  const u = UTF8ToString(url);
  try {
    // Try HEAD first
    let resp = await fetch(u, { method: 'HEAD' });
    if (!resp.ok && resp.status !== 405) { // some CDNs block HEAD
      // try GET but only read headers quickly (we will not stream the body fully)
      resp = await fetch(u, { method: 'GET' });
    }
    if (!resp.ok) return 0;
    const cl = resp.headers.get('content-length');
    if (cl) {
      // Cap to 2^31-1 to fit int, return in bytes
      let n = Number(cl);
      if (!Number.isFinite(n) || n < 0) return 0;
      if (n > 0x7fffffff) n = 0x7fffffff;
      return n | 0;
    }
    // If no content-length, last resort: for GET we can read arrayBuffer length (may download!)
    if (resp.body && resp.headers.get('accept-ranges') !== 'bytes') {
      const ab = await resp.arrayBuffer();
      let n = ab.byteLength;
      if (n > 0x7fffffff) n = 0x7fffffff;
      return n | 0;
    }
    return 0;
  } catch (e) {
    return 0;
  }
});
#endif

size_t ModelFileUtils::getActualFileSizeFromUrl(const std::string& url) {
#ifndef __EMSCRIPTEN__
    return curl_head_size(url);
#else
    int n = wasm_head_size(url.c_str());
    return n > 0 ? static_cast<size_t>(n) : 0;
#endif
}

// ---------- Embind (JS helpers) ----------
#ifdef __EMSCRIPTEN__
static emscripten::val toJS(const MemoryUsage& u) {
    emscripten::val o = emscripten::val::object();
    o.set("modelSizeMB",     emscripten::val((double)u.modelSizeMB));
    o.set("kvCacheMB",       emscripten::val((double)u.kvCacheMB));
    o.set("totalRequiredMB", emscripten::val((double)u.totalRequiredMB));
    o.set("displayString",   emscripten::val(u.displayString));
    o.set("hasEstimate",     emscripten::val(u.hasEstimate));
    o.set("isLoading",       emscripten::val(u.isLoading));
    return o;
}

emscripten::val calcMemoryFromUrl(const std::string& modelId,
                                  const std::string& filename,
                                  const std::string& url,
                                  int contextSize) {
    ModelFile mf;
    mf.modelId = modelId;
    mf.filename = filename;
    mf.downloadUrl = url;
    mf.quant = ModelFileUtils::detectQuantization(filename); // << fix
    mf.memoryUsage = ModelFileUtils::calculateMemoryUsage(mf, contextSize);
    return toJS(mf.memoryUsage);
}

emscripten::val calcMemoryFromFile(const std::string& modelId,
                                   const std::string& filename,
                                   const std::string& path,
                                   int contextSize) {
    ModelFile mf;
    mf.modelId = modelId;
    mf.filename = path; // MEMFS path
    mf.quant = ModelFileUtils::detectQuantization(filename); // << fix
    mf.memoryUsage = ModelFileUtils::calculateMemoryUsage(mf, contextSize);
    return toJS(mf.memoryUsage);
}


EMSCRIPTEN_BINDINGS(model_file_bindings) {
    emscripten::function("calcMemoryFromUrl",  &calcMemoryFromUrl);
    emscripten::function("calcMemoryFromFile", &calcMemoryFromFile);
}
#endif
