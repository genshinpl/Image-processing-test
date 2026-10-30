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

// 3. 朴素高斯滤波 (二维卷积)
vector<float> gaussianFilter(const vector<unsigned char>& img, int width, int height) {
    vector<float> output(width * height, 0.0f);
    float kernel[3][3] = {
        {0.075f, 0.124f, 0.075f},
        {0.124f, 0.204f, 0.124f},
        {0.075f, 0.124f, 0.075f}
    };
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            float sum = 0.0f;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    sum += img[(y + ky) * width + (x + kx)] * kernel[ky + 1][kx + 1];
                }
            }
            output[y * width + x] = sum;
        }
    }
    return output;
}

// 4. 朴素 Sobel 边缘提取
vector<unsigned char> sobelEdge(const vector<float>& img, int width, int height, float threshold) {
    vector<unsigned char> edgeImg(width * height, 0);
    int Gx[3][3] = { {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };
    int Gy[3][3] = { {-1, -2, -1}, {0, 0, 0}, {1, 2, 1} };

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            float gx = 0.0f, gy = 0.0f;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    gx += img[(y + ky) * width + (x + kx)] * Gx[ky + 1][kx + 1];
                    gy += img[(y + ky) * width + (x + kx)] * Gy[ky + 1][kx + 1];
                }
            }
            float mag = std::sqrt(gx * gx + gy * gy); // 直接开方
            edgeImg[y * width + x] = (mag > threshold) ? 255 : 0;
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

    vector<float> blurredImg = gaussianFilter(img, width, height);
    vector<unsigned char> edges = sobelEdge(blurredImg, width, height, 150.0f);

    auto end = std::chrono::high_resolution_clock::now();
    cout << "基础版耗时: " << std::chrono::duration<double, std::milli>(end - start).count() << " ms" << endl;

    cout << "处理完成，提取的边缘像素数: " << count(edges.begin(), edges.end(), 255) << endl;
    return 0;
}