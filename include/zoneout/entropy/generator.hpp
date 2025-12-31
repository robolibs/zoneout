// MIT License
//
// Copyright(c) 2023 Jordan Peck (jordan.me2@gmail.com)
// Copyright(c) 2023 Contributors
// Copyright(c) 2025 Trim Bresilla (trim.bresilla@gmail.com)

// midified version of https://github.com/Auburn/FastNoiseLite

#pragma once
#include <cmath>

namespace entropy {
    namespace noise {

        class NoiseGen {
          public:
            enum NoiseType {
                NoiseType_OpenSimplex2,
                NoiseType_OpenSimplex2S,
                NoiseType_Cellular,
                NoiseType_Perlin,
                NoiseType_ValueCubic,
                NoiseType_Value
            };

            enum RotationType3D { RotationType3D_None, RotationType3D_ImproveXYPlanes, RotationType3D_ImproveXZPlanes };

            enum FractalType {
                FractalType_None,
                FractalType_FBm,
                FractalType_Ridged,
                FractalType_PingPong,
                FractalType_DomainWarpProgressive,
                FractalType_DomainWarpIndependent
            };

            enum CellularDistanceFunction {
                CellularDistanceFunction_Euclidean,
                CellularDistanceFunction_EuclideanSq,
                CellularDistanceFunction_Manhattan,
                CellularDistanceFunction_Hybrid
            };

            enum CellularReturnType {
                CellularReturnType_CellValue,
                CellularReturnType_Distance,
                CellularReturnType_Distance2,
                CellularReturnType_Distance2Add,
                CellularReturnType_Distance2Sub,
                CellularReturnType_Distance2Mul,
                CellularReturnType_Distance2Div
            };

            enum DomainWarpType {
                DomainWarpType_OpenSimplex2,
                DomainWarpType_OpenSimplex2Reduced,
                DomainWarpType_BasicGrid
            };

            NoiseGen(int seed = 1337);

            void SetSeed(int seed);

            void SetFrequency(float frequency);

            void SetNoiseType(NoiseType noiseType);

            void SetRotationType3D(RotationType3D rotationType3D);

            void SetFractalType(FractalType fractalType);

            void SetFractalOctaves(int octaves);

            void SetFractalLacunarity(float lacunarity);

            void SetFractalGain(float gain);

            void SetFractalWeightedStrength(float weightedStrength);

            void SetFractalPingPongStrength(float pingPongStrength);

            void SetCellularDistanceFunction(CellularDistanceFunction cellularDistanceFunction);

            void SetCellularReturnType(CellularReturnType cellularReturnType);

            void SetCellularJitter(float cellularJitter);

            void SetDomainWarpType(DomainWarpType domainWarpType);

            void SetDomainWarpAmp(float domainWarpAmp);

            float GetNoise(float x, float y) const;

            float GetNoise(float x, float y, float z) const;

            void DomainWarp(float &x, float &y) const;

            void DomainWarp(float &x, float &y, float &z) const;

          private:
            enum TransformType3D {
                TransformType3D_None,
                TransformType3D_ImproveXYPlanes,
                TransformType3D_ImproveXZPlanes,
                TransformType3D_DefaultOpenSimplex2
            };

            int mSeed;
            float mFrequency;
            NoiseType mNoiseType;
            RotationType3D mRotationType3D;
            TransformType3D mTransformType3D;

            FractalType mFractalType;
            int mOctaves;
            float mLacunarity;
            float mGain;
            float mWeightedStrength;
            float mPingPongStrength;

            float mFractalBounding;

            CellularDistanceFunction mCellularDistanceFunction;
            CellularReturnType mCellularReturnType;
            float mCellularJitterModifier;

            DomainWarpType mDomainWarpType;
            TransformType3D mWarpTransformType3D;
            float mDomainWarpAmp;

            struct Lookup {
                static const float Gradients2D[];
                static const float Gradients3D[];
                static const float RandVecs2D[];
                static const float RandVecs3D[];
            };

            static float FastMin(float a, float b);

            static float FastMax(float a, float b);

            static float FastAbs(float f);

            static float FastSqrt(float f);

            static int FastFloor(float f);

            static int FastRound(float f);

            static float Lerp(float a, float b, float t);

            static float InterpHermite(float t);

            static float InterpQuintic(float t);

            static float CubicLerp(float a, float b, float c, float d, float t);

            static float PingPong(float t);

            void CalculateFractalBounding();

            void UpdateTransformType3D();

            void UpdateWarpTransformType3D();

            float GenNoiseSingle(int seed, float x, float y) const;

            float GenNoiseSingle(int seed, float x, float y, float z) const;

            void TransformNoiseCoordinate(float &x, float &y) const;

            void TransformNoiseCoordinate(float &x, float &y, float &z) const;

            float GenFractalFBm(float x, float y) const;

            float GenFractalFBm(float x, float y, float z) const;

            float GenFractalRidged(float x, float y) const;

            float GenFractalRidged(float x, float y, float z) const;

            float GenFractalPingPong(float x, float y) const;

            float GenFractalPingPong(float x, float y, float z) const;

            void DomainWarpSingle(float &x, float &y) const;

            void DomainWarpSingle(float &x, float &y, float &z) const;

            void DomainWarpFractalProgressive(float &x, float &y) const;

            void DomainWarpFractalProgressive(float &x, float &y, float &z) const;

            void DomainWarpFractalIndependent(float &x, float &y) const;

            void DomainWarpFractalIndependent(float &x, float &y, float &z) const;

            void TransformDomainWarpCoordinate(float &x, float &y) const;

            void TransformDomainWarpCoordinate(float &x, float &y, float &z) const;

            float SingleSimplex(int seed, float x, float y) const;

            float SingleOpenSimplex2(int seed, float x, float y, float z) const;

            float SingleOpenSimplex2S(int seed, float x, float y) const;

            float SingleOpenSimplex2S(int seed, float x, float y, float z) const;

            float SingleCellular(int seed, float x, float y) const;

            float SingleCellular(int seed, float x, float y, float z) const;

            float SinglePerlin(int seed, float x, float y) const;

            float SinglePerlin(int seed, float x, float y, float z) const;

            float SingleValueCubic(int seed, float x, float y) const;

            float SingleValueCubic(int seed, float x, float y, float z) const;

            float SingleValue(int seed, float x, float y) const;

            float SingleValue(int seed, float x, float y, float z) const;

            void DoSingleDomainWarp(int seed, float amp, float freq, float x, float y, float &xr, float &yr) const;

            void DoSingleDomainWarp(int seed, float amp, float freq, float x, float y, float z, float &xr, float &yr,
                                    float &zr) const;

            void SingleDomainWarpBasicGrid(int seed, float warpAmp, float frequency, float x, float y, float &xr,
                                           float &yr) const;

            void SingleDomainWarpBasicGrid(int seed, float warpAmp, float frequency, float x, float y, float z,
                                           float &xr, float &yr, float &zr) const;

            void SingleDomainWarpSimplexGradient(int seed, float warpAmp, float frequency, float x, float y, float &xr,
                                                 float &yr, bool outGradOnly) const;

            void SingleDomainWarpOpenSimplex2Gradient(int seed, float warpAmp, float frequency, float x, float y,
                                                      float z, float &xr, float &yr, float &zr, bool outGradOnly) const;

            static int Hash(int seed, int xPrimed, int yPrimed);

            static int Hash(int seed, int xPrimed, int yPrimed, int zPrimed);

            static float ValCoord(int seed, int xPrimed, int yPrimed);

            static float ValCoord(int seed, int xPrimed, int yPrimed, int zPrimed);

            float GradCoord(int seed, int xPrimed, int yPrimed, float xd, float yd) const;

            float GradCoord(int seed, int xPrimed, int yPrimed, int zPrimed, float xd, float yd, float zd) const;

            void GradCoordOut(int seed, int xPrimed, int yPrimed, float &xo, float &yo) const;

            void GradCoordOut(int seed, int xPrimed, int yPrimed, int zPrimed, float &xo, float &yo, float &zo) const;

            void GradCoordDual(int seed, int xPrimed, int yPrimed, float xd, float yd, float &xo, float &yo) const;

            void GradCoordDual(int seed, int xPrimed, int yPrimed, int zPrimed, float xd, float yd, float zd, float &xo,
                               float &yo, float &zo) const;
        };

    } // namespace noise
} // namespace entropy
