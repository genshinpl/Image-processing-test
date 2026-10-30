#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <chrono>

using namespace std;

#pragma pack(push, 1) // 防止结构体对齐导致读取 BMP 报错
struct BMPHeader {
    char signature[2];
    int fileSize;
    int reserved;
    int dataOffset;
};
struct BMPInfoHeader {
    int headerSize;
    int width;
    int height;
    short planes;
    short bitCount;
    int compression;
    int imageSize;
    int xPixelsPerMeter;
    int yPixelsPerMeter;
    int colorsUsed;
    int colorsImportant;
};
#pragma pack(pop)

// 1. BMP 读取与保存
vector<unsigned char> readBMP(const char* filename, int& width, int& height) {
    ifstream file(filename, ios::binary);
    if (!file) { cerr << "打开文件失败!" << endl; exit(1); }

    BMPHeader bfh;
    file.read((char*)&bfh, sizeof(bfh));
    BMPInfoHeader bih;
    file.read((char*)&bih, sizeof(bih));

    width = bih.width;
    height = bih.height;
    int rowSize = (width * 3 + 3) & (~3); // 4字节对齐
    vector<unsigned char> img(height * width);
    vector<unsigned char> rowData(rowSize);

    file.seekg(bfh.dataOffset, ios::beg);
    for (int y = height - 1; y >= 0; --y) { // BMP 倒序存储
        file.read((char*)rowData.data(), rowSize);
        for (int x = 0; x < width; ++x) {
            unsigned char B = rowData[x * 3];
            unsigned char G = rowData[x * 3 + 1];
            unsigned char R = rowData[x * 3 + 2];
            // 转换为灰度图：Y = 0.299R + 0.587G + 0.114B
            img[y * width + x] = (unsigned char)(0.299 * R + 0.587 * G + 0.114 * B);
        }
    }
    return img;
}

// 2. 添加噪声
void addNoise(vector<unsigned char>& img, int width, int height) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < width * height; ++i) {
        int noise = rand() % 100 - 50; // 随机噪声 [-50, 50]
        int val = img[i] + noise;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        img[i] = (unsigned char)val;
    }
}

/// 优化 1：可分离高斯卷积 + 一维指针遍历
vector<float> gaussianFilter_Fast(const vector<unsigned char>& img, int width, int height) {
    vector<float> temp(width * height, 0.0f);
    vector<float> output(width * height, 0.0f);
    const float k[3] = { 0.274f, 0.452f, 0.274f }; // 降维为一维核

    // 横向 1D 卷积
    for (int y = 0; y < height; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            temp[y * width + x] = img[y * width + x - 1] * k[0] +
                img[y * width + x] * k[1] +
                img[y * width + x + 1] * k[2];
        }
    }
    // 纵向 1D 卷积
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 0; x < width; ++x) {
            output[y * width + x] = temp[(y - 1) * width + x] * k[0] +
                temp[y * width + x] * k[1] +
                temp[(y + 1) * width + x] * k[2];
        }
    }
    return output;
}

// 优化 2：消除 sqrt + 手动指针展开
vector<unsigned char> sobelEdge_Fast(const vector<float>& img, int width, int height, float threshold) {
    vector<unsigned char> edgeImg(width * height, 0);
    float threshSq = threshold * threshold; // 用平方代替开方

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            int idx = y * width + x;
            // 预取周围 8 个像素，避免重复计算索引
            float tl = img[idx - width - 1], tc = img[idx - width], tr = img[idx - width + 1];
            float ml = img[idx - 1], mr = img[idx + 1];
            float bl = img[idx + width - 1], bc = img[idx + width], br = img[idx + width + 1];

            // Sobel 卷积展开
            float gx = (tr + 2 * mr + br) - (tl + 2 * ml + bl);
            float gy = (bl + 2 * bc + br) - (tl + 2 * tc + tr);

            float magSq = gx * gx + gy * gy; // 保留平方
            edgeImg[idx] = (magSq > threshSq) ? 255 : 0;
        }
    }
    return edgeImg;
}

// 5. 主函数
int main() {
    int width, height;
    string filename = "input.bmp"; // 请准备一张 1024x1024 的图
    vector<unsigned char> img = readBMP(filename.c_str(), width, height);
    if (img.empty()) return 0;

    addNoise(img, width, height);

    // 开始计时
    auto start = std::chrono::high_resolution_clock::now();

    vector<float> blurredImg = gaussianFilter_Fast(img, width, height);
    vector<unsigned char> edges = sobelEdge_Fast(blurredImg, width, height, 150.0f);

    auto end = std::chrono::high_resolution_clock::now();
    cout << "基础版耗时: " << std::chrono::duration<double, std::milli>(end - start).count() << " ms" << endl;

    cout << "处理完成，提取的边缘像素数: " << count(edges.begin(), edges.end(), 255) << endl;
    return 0;
}