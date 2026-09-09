#include "../fasttext_demo.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <sstream>
#include <string>

#include "../vendor/fastText/src/args.h"
#include "../vendor/fastText/src/fasttext.h"

struct ftdemo_model {
    fasttext::FastText tFastText;
    int iDim;
};

static std::string g_sLastError;

static void ftdemo__set_error(const std::string &sMessage)
{
    g_sLastError = sMessage;
}

static void ftdemo__set_error_cstr(const char *sMessage)
{
    g_sLastError = sMessage ? sMessage : "unknown error";
}

static bool ftdemo__validate_text(const char *sText)
{
    return sText && sText[0] != '\0';
}

int ftdemo_train_default_model(
    const char *sInputPath,
    const char *sOutputPrefix
)
{
    fasttext::Args tArgs;
    fasttext::FastText tFastText;

    if ( !ftdemo__validate_text(sInputPath) || !ftdemo__validate_text(sOutputPrefix) ) {
        ftdemo__set_error_cstr("training input path or output prefix is empty");
        return -1;
    }

    try {
        tArgs.input = sInputPath;
        tArgs.output = sOutputPrefix;
        tArgs.model = fasttext::model_name::sg;
        tArgs.loss = fasttext::loss_name::ns;
        tArgs.dim = 32;
        tArgs.ws = 5;
        tArgs.epoch = 35;
        tArgs.minCount = 1;
        tArgs.wordNgrams = 2;
        tArgs.bucket = 100000;
        tArgs.minn = 2;
        tArgs.maxn = 5;
        tArgs.thread = 1;
        tArgs.verbose = 1;
        tArgs.lr = 0.05;
        tArgs.neg = 10;
        tArgs.seed = 7;

        tFastText.train(tArgs);
        tFastText.saveModel(std::string(sOutputPrefix) + ".bin");
        tFastText.saveVectors(std::string(sOutputPrefix) + ".vec");
        g_sLastError.clear();
        return 0;
    } catch ( const std::exception &tEx ) {
        ftdemo__set_error(tEx.what());
        return -1;
    } catch (...) {
        ftdemo__set_error_cstr("unknown fastText training error");
        return -1;
    }
}

int ftdemo_model_load(
    const char *sModelPath,
    ftdemo_model **ppModel
)
{
    std::unique_ptr<ftdemo_model> pModel;

    if ( !ppModel ) {
        ftdemo__set_error_cstr("output model pointer is null");
        return -1;
    }

    *ppModel = NULL;
    if ( !ftdemo__validate_text(sModelPath) ) {
        ftdemo__set_error_cstr("model path is empty");
        return -1;
    }

    try {
        pModel.reset(new ftdemo_model());
        pModel->tFastText.loadModel(sModelPath);
        pModel->iDim = pModel->tFastText.getDimension();
        *ppModel = pModel.release();
        g_sLastError.clear();
        return 0;
    } catch ( const std::bad_alloc & ) {
        ftdemo__set_error_cstr("out of memory while loading fastText model");
        return -1;
    } catch ( const std::exception &tEx ) {
        ftdemo__set_error(tEx.what());
        return -1;
    } catch (...) {
        ftdemo__set_error_cstr("unknown fastText load error");
        return -1;
    }
}

void ftdemo_model_destroy(ftdemo_model *pModel)
{
    delete pModel;
}

int ftdemo_model_dimension(const ftdemo_model *pModel)
{
    if ( !pModel ) {
        return -1;
    }
    return pModel->iDim;
}

int ftdemo_model_embed_text(
    ftdemo_model *pModel,
    const char *sText,
    float *pOutVector,
    size_t iOutVectorCount
)
{
    fasttext::Vector tVector(1);
    std::istringstream tInput;
    size_t i;

    if ( !pModel || !pOutVector ) {
        ftdemo__set_error_cstr("model or output vector pointer is null");
        return -1;
    }
    if ( !ftdemo__validate_text(sText) ) {
        ftdemo__set_error_cstr("input text is empty");
        return -1;
    }
    if ( iOutVectorCount != (size_t)pModel->iDim ) {
        ftdemo__set_error_cstr("output vector size does not match model dimension");
        return -1;
    }

    try {
        tVector = fasttext::Vector(pModel->iDim);
        tInput.str(sText);
        pModel->tFastText.getSentenceVector(tInput, tVector);

        for ( i = 0; i < iOutVectorCount; ++i ) {
            pOutVector[i] = tVector[(int64_t)i];
        }
        g_sLastError.clear();
        return 0;
    } catch ( const std::exception &tEx ) {
        ftdemo__set_error(tEx.what());
        return -1;
    } catch (...) {
        ftdemo__set_error_cstr("unknown fastText embed error");
        return -1;
    }
}

float ftdemo_cosine_similarity(
    const float *pLeft,
    const float *pRight,
    size_t iCount
)
{
    double fDot = 0.0;
    double fLeftNorm = 0.0;
    double fRightNorm = 0.0;
    size_t i;

    if ( !pLeft || !pRight || iCount == 0u ) {
        return 0.0f;
    }

    for ( i = 0; i < iCount; ++i ) {
        double fL = (double)pLeft[i];
        double fR = (double)pRight[i];
        fDot += fL * fR;
        fLeftNorm += fL * fL;
        fRightNorm += fR * fR;
    }

    if ( fLeftNorm <= 0.0 || fRightNorm <= 0.0 ) {
        return 0.0f;
    }

    return (float)(fDot / (std::sqrt(fLeftNorm) * std::sqrt(fRightNorm)));
}

int ftdemo_model_compare_texts(
    ftdemo_model *pModel,
    const char *sLeftText,
    const char *sRightText,
    float *pfSimilarity
)
{
    std::unique_ptr<float[]> pLeft;
    std::unique_ptr<float[]> pRight;
    size_t iDim;

    if ( !pModel || !pfSimilarity ) {
        ftdemo__set_error_cstr("model or similarity pointer is null");
        return -1;
    }
    if ( !ftdemo__validate_text(sLeftText) || !ftdemo__validate_text(sRightText) ) {
        ftdemo__set_error_cstr("comparison text is empty");
        return -1;
    }

    iDim = (size_t)pModel->iDim;
    try {
        pLeft.reset(new float[iDim]);
        pRight.reset(new float[iDim]);
    } catch ( const std::bad_alloc & ) {
        ftdemo__set_error_cstr("out of memory while comparing texts");
        return -1;
    }

    if ( ftdemo_model_embed_text(pModel, sLeftText, pLeft.get(), iDim) != 0 ) {
        return -1;
    }
    if ( ftdemo_model_embed_text(pModel, sRightText, pRight.get(), iDim) != 0 ) {
        return -1;
    }

    *pfSimilarity = ftdemo_cosine_similarity(pLeft.get(), pRight.get(), iDim);
    g_sLastError.clear();
    return 0;
}

const char *ftdemo_last_error(void)
{
    return g_sLastError.empty() ? "" : g_sLastError.c_str();
}
