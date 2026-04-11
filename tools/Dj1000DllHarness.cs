using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

internal static class Dj1000DllHarness
{
    private const int ExpectedDatSize = 0x20000;
    private const int RawBlockSize = 0x1F600;
    private const int TrailerSize = 13;
    private const int WorkingBufferSize = 0x8DC28;
    private const uint MemCommit = 0x1000;
    private const uint MemReserve = 0x2000;
    private const uint PageExecuteReadWrite = 0x40;
    private static readonly byte[] ExpectedSignature = { 0xC4, 0xB2, 0xE3, 0x22 };
    private static readonly List<string> TraceLines = new List<string>();
    private static string TraceOutputPrefix = string.Empty;
    private static int SourcePrepass4570CallCount = 0;
    private static int PostGeometryPrepareCallCount = 0;
    private static int PostGeometryStage4810CallCount = 0;
    private static int PostGeometryCenterScaleCallCount = 0;
    private static int PostGeometryStage2DD0CallCount = 0;
    private static int PostGeometryStage3890CallCount = 0;
    private static int PostGeometryStage3060CallCount = 0;
    private static int PostGeometryDualScaleCallCount = 0;
    private static int PostRgbStage42A0CallCount = 0;
    private static int PostRgbStage3B00CallCount = 0;
    private static int PostRgbStage40F0CallCount = 0;

    private static HookPatch SourcePrepass4570Hook;
    private static HookPatch PostGeometryPrepareHook;
    private static HookPatch PostGeometryStage4810Hook;
    private static HookPatch PostGeometryCenterScaleHook;
    private static HookPatch PostGeometryStage2DD0Hook;
    private static HookPatch PostGeometryStage3890Hook;
    private static HookPatch PostGeometryStage3060Hook;
    private static HookPatch PostGeometryDualScaleHook;
    private static HookPatch PostRgbStage42A0Hook;
    private static HookPatch PostRgbStage3B00Hook;
    private static HookPatch PostRgbStage40F0Hook;

    private static InternalSourcePrepass4570Delegate SourcePrepass4570Trampoline;
    private static InternalPostGeometryPrepareDelegate PostGeometryPrepareTrampoline;
    private static InternalPostGeometryStage4810Delegate PostGeometryStage4810Trampoline;
    private static InternalPostGeometryCenterScaleDelegate PostGeometryCenterScaleTrampoline;
    private static InternalPostGeometryStage2DD0Delegate PostGeometryStage2DD0Trampoline;
    private static InternalPostGeometryStage3890Delegate PostGeometryStage3890Trampoline;
    private static InternalPostGeometryStage3060Delegate PostGeometryStage3060Trampoline;
    private static InternalPostGeometryDualScaleDelegate PostGeometryDualScaleTrampoline;
    private static InternalPostRgbStage42A0Delegate PostRgbStage42A0Trampoline;
    private static InternalPostRgbStage3B00Delegate PostRgbStage3B00Trampoline;
    private static InternalPostRgbStage40F0Delegate PostRgbStage40F0Trampoline;

    private static InternalSourcePrepass4570Delegate SourcePrepass4570HookDelegate;
    private static InternalPostGeometryPrepareDelegate PostGeometryPrepareHookDelegate;
    private static InternalPostGeometryStage4810Delegate PostGeometryStage4810HookDelegate;
    private static InternalPostGeometryCenterScaleDelegate PostGeometryCenterScaleHookDelegate;
    private static InternalPostGeometryStage2DD0Delegate PostGeometryStage2DD0HookDelegate;
    private static InternalPostGeometryStage3890Delegate PostGeometryStage3890HookDelegate;
    private static InternalPostGeometryStage3060Delegate PostGeometryStage3060HookDelegate;
    private static InternalPostGeometryDualScaleDelegate PostGeometryDualScaleHookDelegate;
    private static InternalPostRgbStage42A0Delegate PostRgbStage42A0HookDelegate;
    private static InternalPostRgbStage3B00Delegate PostRgbStage3B00HookDelegate;
    private static InternalPostRgbStage40F0Delegate PostRgbStage40F0HookDelegate;

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool VirtualProtect(
        IntPtr lpAddress,
        UIntPtr dwSize,
        uint flNewProtect,
        out uint lpflOldProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr VirtualAlloc(
        IntPtr lpAddress,
        UIntPtr dwSize,
        uint flAllocationType,
        uint flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr GetCurrentProcess();

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FlushInstructionCache(
        IntPtr hProcess,
        IntPtr lpBaseAddress,
        UIntPtr dwSize);

    private sealed class HookPatch
    {
        public IntPtr Target;
        public int Length;
        public byte[] OriginalBytes;
        public IntPtr Trampoline;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int Dj1000GraphicConvDelegate(
        int mode,
        IntPtr imageBuffer,
        IntPtr settings);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalResampleLutDelegate(
        IntPtr outputBuffer,
        int widthParameter,
        int unused);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalGeometryStageDelegate(
        int previewFlag,
        int exportMode,
        int geometrySelector,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalRowResampleDelegate(
        IntPtr sourceRow,
        IntPtr outputRow,
        IntPtr lut,
        int resetCounter,
        int modeFlag,
        int widthLimit,
        int outputLength);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalNormalizeStageDelegate(
        int previewFlag,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalSourceStageDelegate(
        int previewFlag,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2,
        IntPtr sourceBytes);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalSourcePrepass4570Delegate(
        int previewFlag,
        IntPtr planeBytes);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostGeometryPrepareDelegate(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2,
        IntPtr delta0,
        IntPtr center,
        IntPtr delta2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryFilterDelegate(
        int width,
        int height,
        IntPtr delta0,
        IntPtr delta2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostGeometryCenterScaleDelegate(
        int width,
        int height,
        IntPtr delta0,
        IntPtr center,
        IntPtr delta2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostGeometryRgbConvertDelegate(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2,
        IntPtr input0,
        IntPtr input1,
        IntPtr input2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryEdgeResponseDelegate(
        int width,
        int height,
        IntPtr output,
        IntPtr input);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostGeometryDualScaleDelegate(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        double scalar);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryStage2A00Delegate(
        int width,
        int height,
        IntPtr unused0,
        IntPtr plane,
        IntPtr unused1);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryStage2DD0Delegate(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryStage4810Delegate(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryStage3600Delegate(
        int width,
        int height,
        IntPtr plane0);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostGeometryStage3890Delegate(
        int width,
        int height,
        int level,
        IntPtr plane0,
        IntPtr plane1);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int InternalPostGeometryStage3060Delegate(
        int width,
        int height,
        int stageParam0,
        int stageParam1,
        int stageParam2,
        int stageParam3,
        IntPtr plane0,
        double scalar,
        int unused,
        IntPtr plane1,
        int threshold);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostRgbStage3B00Delegate(
        int width,
        int height,
        int level,
        double scalar,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostRgbStage42A0Delegate(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2,
        int scale0,
        int scale1,
        int scale2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostRgbStage40F0Delegate(
        int width,
        int height,
        int level,
        int selector,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InternalPostRgbStage3F40Delegate(
        int width,
        int height,
        int unusedLevel,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2);

    private sealed class Options
    {
        public string DllPath;
        public string InputPath;
        public string OutputPath;
        public int ConverterMode = 0;
        public int ExportMode = 5;
        public int[] Settings = { 100, 100, 100, 3, 3, 3, 3 };
        public bool ForceUnsignedTrailer = false;
        public int? DumpResampleLutWidth = null;
        public int HelperModeE4 = 0;
        public bool DumpGeometryStage = false;
        public int GeometryPreviewFlag = 0;
        public int GeometryExportMode = 5;
        public int GeometrySelector = 1;
        public string Plane0Path;
        public string Plane1Path;
        public string Plane2Path;
        public bool DumpRowResample = false;
        public string RowInputPath;
        public int RowLutWidth = 0x101;
        public int RowResetCounter = 0x100;
        public int RowModeFlag = 0;
        public int RowWidthLimit = 0x1F9;
        public int RowOutputLength = 0x144;
        public int RowModeE0 = 1;
        public int RowHelperModeE4 = 1;
        public bool DumpNormalizeStage = false;
        public int NormalizePreviewFlag = 0;
        public int NormalizeStride = 0;
        public int NormalizeRows = 0;
        public bool DumpSourceStage = false;
        public bool DumpSourcePrepass4570 = false;
        public int SourcePreviewFlag = 0;
        public int SourceOutputStride = 0;
        public int SourceOutputRows = 0;
        public string SourceInputPath;
        public bool DumpPostGeometryPrepare = false;
        public bool DumpPostGeometryFilter = false;
        public bool DumpPostGeometryCenterScale = false;
        public bool DumpPostGeometryRgbConvert = false;
        public bool DumpPostGeometryEdgeResponse = false;
        public bool DumpPostGeometryDualScale = false;
        public bool DumpPostGeometryStage2A00 = false;
        public bool DumpPostGeometryStage2DD0 = false;
        public bool DumpPostGeometryStage4810 = false;
        public bool DumpPostGeometryStage3600 = false;
        public bool DumpPostGeometryStage3890 = false;
        public bool DumpPostGeometryStage3060 = false;
        public bool DumpPostRgbStage3B00 = false;
        public bool DumpPostRgbStage3F40 = false;
        public bool DumpPostRgbStage42A0 = false;
        public bool DumpPostRgbStage40F0 = false;
        public bool TraceLargeCallsiteParams = false;
        public int StageWidth = 0;
        public int StageHeight = 0;
        public int StageParam0 = 0;
        public int StageParam1 = 0;
        public int StageParam2 = 0;
        public string Delta0Path;
        public string Delta2Path;
        public double Scalar = 0.0;
        public int Level = 3;
        public int Selector = 0;
        public int Threshold = 0;
    }

    private static string FormatPointer(IntPtr value)
    {
        return "0x" + value.ToInt32().ToString("x8");
    }

    private static void TraceLine(string message)
    {
        TraceLines.Add(message);
    }

    private static void DumpTraceBytes(string suffix, IntPtr source, int byteCount)
    {
        if (string.IsNullOrEmpty(TraceOutputPrefix) || byteCount <= 0)
        {
            return;
        }

        byte[] bytes = new byte[byteCount];
        Marshal.Copy(source, bytes, 0, byteCount);
        File.WriteAllBytes(TraceOutputPrefix + suffix, bytes);
    }

    private static void WriteAbsoluteJump(IntPtr target, IntPtr destination, int patchLength)
    {
        byte[] patch = new byte[patchLength];
        patch[0] = 0x68;
        Array.Copy(BitConverter.GetBytes(destination.ToInt32()), 0, patch, 1, 4);
        patch[5] = 0xC3;
        for (int index = 6; index < patch.Length; index++)
        {
            patch[index] = 0x90;
        }

        Marshal.Copy(patch, 0, target, patch.Length);
        FlushInstructionCache(GetCurrentProcess(), target, (UIntPtr)patch.Length);
    }

    private static HookPatch InstallHook(IntPtr target, IntPtr hookPtr, int patchLength)
    {
        byte[] originalBytes = new byte[patchLength];
        Marshal.Copy(target, originalBytes, 0, patchLength);

        IntPtr trampoline = VirtualAlloc(
            IntPtr.Zero,
            (UIntPtr)(patchLength + 6),
            MemCommit | MemReserve,
            PageExecuteReadWrite);
        if (trampoline == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "VirtualAlloc failed for trampoline (error " + Marshal.GetLastWin32Error() + ").");
        }

        Marshal.Copy(originalBytes, 0, trampoline, patchLength);
        WriteAbsoluteJump(IntPtr.Add(trampoline, patchLength), IntPtr.Add(target, patchLength), 6);

        uint oldProtect;
        if (!VirtualProtect(target, (UIntPtr)patchLength, PageExecuteReadWrite, out oldProtect))
        {
            throw new InvalidOperationException(
                "VirtualProtect failed for hook install (error " + Marshal.GetLastWin32Error() + ").");
        }

        WriteAbsoluteJump(target, hookPtr, patchLength);

        uint restoreProtect;
        if (!VirtualProtect(target, (UIntPtr)patchLength, oldProtect, out restoreProtect))
        {
            throw new InvalidOperationException(
                "VirtualProtect failed while restoring page protection (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        return new HookPatch
        {
            Target = target,
            Length = patchLength,
            OriginalBytes = originalBytes,
            Trampoline = trampoline,
        };
    }

    private static void RestoreHook(HookPatch hook)
    {
        if (hook == null)
        {
            return;
        }

        uint oldProtect;
        if (!VirtualProtect(hook.Target, (UIntPtr)hook.Length, PageExecuteReadWrite, out oldProtect))
        {
            throw new InvalidOperationException(
                "VirtualProtect failed for hook restore (error " + Marshal.GetLastWin32Error() + ").");
        }

        Marshal.Copy(hook.OriginalBytes, 0, hook.Target, hook.Length);
        FlushInstructionCache(GetCurrentProcess(), hook.Target, (UIntPtr)hook.Length);

        uint restoreProtect;
        if (!VirtualProtect(hook.Target, (UIntPtr)hook.Length, oldProtect, out restoreProtect))
        {
            throw new InvalidOperationException(
                "VirtualProtect failed while restoring hook page protection (error " +
                Marshal.GetLastWin32Error() + ").");
        }
    }

    private static int HookSourcePrepass4570(int previewFlag, IntPtr planeBytes)
    {
        SourcePrepass4570CallCount++;
        TraceLine(
            "4570 call=" + SourcePrepass4570CallCount +
            " preview=" + previewFlag +
            " plane=" + FormatPointer(planeBytes));
        return SourcePrepass4570Trampoline(previewFlag, planeBytes);
    }

    private static void HookPostGeometryPrepare(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2,
        IntPtr delta0,
        IntPtr center,
        IntPtr delta2)
    {
        PostGeometryPrepareCallCount++;
        TraceLine(
            "2410 call=" + PostGeometryPrepareCallCount +
            " width=" + width +
            " height=" + height +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1) +
            " plane2=" + FormatPointer(plane2) +
            " delta0=" + FormatPointer(delta0) +
            " center=" + FormatPointer(center) +
            " delta2=" + FormatPointer(delta2));
        int inputBytes = width * height;
        int outputBytes = width * height * 8;
        DumpTraceBytes(".2410.pre.plane0.bin", plane0, inputBytes);
        DumpTraceBytes(".2410.pre.plane1.bin", plane1, inputBytes);
        DumpTraceBytes(".2410.pre.plane2.bin", plane2, inputBytes);
        PostGeometryPrepareTrampoline(width, height, plane0, plane1, plane2, delta0, center, delta2);
        DumpTraceBytes(".2410.post.delta0.f64", delta0, outputBytes);
        DumpTraceBytes(".2410.post.center.f64", center, outputBytes);
        DumpTraceBytes(".2410.post.delta2.f64", delta2, outputBytes);
    }

    private static int HookPostGeometryStage4810(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2)
    {
        PostGeometryStage4810CallCount++;
        TraceLine(
            "4810 call=" + PostGeometryStage4810CallCount +
            " width=" + width +
            " height=" + height +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1) +
            " plane2=" + FormatPointer(plane2));
        int planeBytes = width * height * 8;
        DumpTraceBytes(".4810.pre.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".4810.pre.plane1.f64", plane1, planeBytes);
        DumpTraceBytes(".4810.pre.plane2.f64", plane2, planeBytes);
        int result = PostGeometryStage4810Trampoline(width, height, plane0, plane1, plane2);
        DumpTraceBytes(".4810.post.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".4810.post.plane1.f64", plane1, planeBytes);
        DumpTraceBytes(".4810.post.plane2.f64", plane2, planeBytes);
        return result;
    }

    private static void HookPostGeometryCenterScale(
        int width,
        int height,
        IntPtr delta0,
        IntPtr center,
        IntPtr delta2)
    {
        PostGeometryCenterScaleCallCount++;
        TraceLine(
            "2c40 call=" + PostGeometryCenterScaleCallCount +
            " width=" + width +
            " height=" + height +
            " delta0=" + FormatPointer(delta0) +
            " center=" + FormatPointer(center) +
            " delta2=" + FormatPointer(delta2));
        int planeBytes = width * height * 8;
        DumpTraceBytes(".2c40.pre.delta0.f64", delta0, planeBytes);
        DumpTraceBytes(".2c40.pre.center.f64", center, planeBytes);
        DumpTraceBytes(".2c40.pre.delta2.f64", delta2, planeBytes);
        PostGeometryCenterScaleTrampoline(width, height, delta0, center, delta2);
        DumpTraceBytes(".2c40.post.delta0.f64", delta0, planeBytes);
        DumpTraceBytes(".2c40.post.center.f64", center, planeBytes);
        DumpTraceBytes(".2c40.post.delta2.f64", delta2, planeBytes);
    }

    private static int HookPostGeometryStage2DD0(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1)
    {
        PostGeometryStage2DD0CallCount++;
        TraceLine(
            "2dd0 call=" + PostGeometryStage2DD0CallCount +
            " width=" + width +
            " height=" + height +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1));
        int planeBytes = width * height * 8;
        DumpTraceBytes(".2dd0.pre.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".2dd0.pre.plane1.f64", plane1, planeBytes);
        int result = PostGeometryStage2DD0Trampoline(width, height, plane0, plane1);
        DumpTraceBytes(".2dd0.post.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".2dd0.post.plane1.f64", plane1, planeBytes);
        return result;
    }

    private static void HookPostGeometryStage3890(
        int width,
        int height,
        int level,
        IntPtr plane0,
        IntPtr plane1)
    {
        PostGeometryStage3890CallCount++;
        TraceLine(
            "3890 call=" + PostGeometryStage3890CallCount +
            " width=" + width +
            " height=" + height +
            " level=" + level +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1));
        int planeBytes = width * height * 8;
        DumpTraceBytes(".3890.pre.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".3890.pre.plane1.f64", plane1, planeBytes);
        PostGeometryStage3890Trampoline(width, height, level, plane0, plane1);
        DumpTraceBytes(".3890.post.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".3890.post.plane1.f64", plane1, planeBytes);
    }

    private static int HookPostGeometryStage3060(
        int width,
        int height,
        int stageParam0,
        int stageParam1,
        int stageParam2,
        int stageParam3,
        IntPtr plane0,
        double scalar,
        int unused,
        IntPtr plane1,
        int threshold)
    {
        PostGeometryStage3060CallCount++;
        TraceLine(
            "3060 call=" + PostGeometryStage3060CallCount +
            " width=" + width +
            " height=" + height +
            " stageParam0=" + stageParam0 +
            " stageParam1=" + stageParam1 +
            " stageParam2=" + stageParam2 +
            " stageParam3=" + stageParam3 +
            " plane0=" + FormatPointer(plane0) +
            " scalar=" + scalar.ToString(System.Globalization.CultureInfo.InvariantCulture) +
            " unused=" + unused +
            " plane1=" + FormatPointer(plane1) +
            " threshold=" + threshold);
        DumpTraceBytes(".3060.pre.plane0.i32", plane0, width * height * 4);
        DumpTraceBytes(".3060.pre.plane1.f64", plane1, width * height * 8);
        int result = PostGeometryStage3060Trampoline(
            width,
            height,
            stageParam0,
            stageParam1,
            stageParam2,
            stageParam3,
            plane0,
            scalar,
            unused,
            plane1,
            threshold);
        DumpTraceBytes(".3060.post.plane1.f64", plane1, width * height * 8);
        return result;
    }

    private static void HookPostGeometryDualScale(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        double scalar)
    {
        PostGeometryDualScaleCallCount++;
        TraceLine(
            "4450 call=" + PostGeometryDualScaleCallCount +
            " width=" + width +
            " height=" + height +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1) +
            " scalar=" + scalar.ToString(System.Globalization.CultureInfo.InvariantCulture));
        int planeBytes = width * height * 8;
        DumpTraceBytes(".4450.pre.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".4450.pre.plane1.f64", plane1, planeBytes);
        PostGeometryDualScaleTrampoline(width, height, plane0, plane1, scalar);
        DumpTraceBytes(".4450.post.plane0.f64", plane0, planeBytes);
        DumpTraceBytes(".4450.post.plane1.f64", plane1, planeBytes);
    }

    private static void HookPostRgbStage42A0(
        int width,
        int height,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2,
        int scale0,
        int scale1,
        int scale2)
    {
        PostRgbStage42A0CallCount++;
        TraceLine(
            "42a0 call=" + PostRgbStage42A0CallCount +
            " width=" + width +
            " height=" + height +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1) +
            " plane2=" + FormatPointer(plane2) +
            " scale0=" + scale0 +
            " scale1=" + scale1 +
            " scale2=" + scale2);
        int planeBytes = width * height;
        string prefix = ".42a0.call" + PostRgbStage42A0CallCount;
        DumpTraceBytes(prefix + ".pre.plane0.bin", plane0, planeBytes);
        DumpTraceBytes(prefix + ".pre.plane1.bin", plane1, planeBytes);
        DumpTraceBytes(prefix + ".pre.plane2.bin", plane2, planeBytes);
        PostRgbStage42A0Trampoline(width, height, plane0, plane1, plane2, scale0, scale1, scale2);
        DumpTraceBytes(prefix + ".post.plane0.bin", plane0, planeBytes);
        DumpTraceBytes(prefix + ".post.plane1.bin", plane1, planeBytes);
        DumpTraceBytes(prefix + ".post.plane2.bin", plane2, planeBytes);
    }

    private static void HookPostRgbStage3B00(
        int width,
        int height,
        int level,
        double scalar,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2)
    {
        PostRgbStage3B00CallCount++;
        TraceLine(
            "3b00 call=" + PostRgbStage3B00CallCount +
            " width=" + width +
            " height=" + height +
            " level=" + level +
            " scalar=" + scalar.ToString(System.Globalization.CultureInfo.InvariantCulture) +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1) +
            " plane2=" + FormatPointer(plane2));
        int planeBytes = width * height;
        string prefix = ".3b00.call" + PostRgbStage3B00CallCount;
        DumpTraceBytes(prefix + ".pre.plane0.bin", plane0, planeBytes);
        DumpTraceBytes(prefix + ".pre.plane1.bin", plane1, planeBytes);
        DumpTraceBytes(prefix + ".pre.plane2.bin", plane2, planeBytes);
        PostRgbStage3B00Trampoline(width, height, level, scalar, plane0, plane1, plane2);
        DumpTraceBytes(prefix + ".post.plane0.bin", plane0, planeBytes);
        DumpTraceBytes(prefix + ".post.plane1.bin", plane1, planeBytes);
        DumpTraceBytes(prefix + ".post.plane2.bin", plane2, planeBytes);
    }

    private static void HookPostRgbStage40F0(
        int width,
        int height,
        int level,
        int selector,
        IntPtr plane0,
        IntPtr plane1,
        IntPtr plane2)
    {
        PostRgbStage40F0CallCount++;
        TraceLine(
            "40f0 call=" + PostRgbStage40F0CallCount +
            " width=" + width +
            " height=" + height +
            " level=" + level +
            " selector=" + selector +
            " plane0=" + FormatPointer(plane0) +
            " plane1=" + FormatPointer(plane1) +
            " plane2=" + FormatPointer(plane2));
        int planeBytes = width * height;
        string prefix =
            ".40f0.call" + PostRgbStage40F0CallCount +
            ".selector" + selector;
        DumpTraceBytes(prefix + ".pre.plane0.bin", plane0, planeBytes);
        DumpTraceBytes(prefix + ".pre.plane1.bin", plane1, planeBytes);
        DumpTraceBytes(prefix + ".pre.plane2.bin", plane2, planeBytes);
        PostRgbStage40F0Trampoline(width, height, level, selector, plane0, plane1, plane2);
        DumpTraceBytes(prefix + ".post.plane0.bin", plane0, planeBytes);
        DumpTraceBytes(prefix + ".post.plane1.bin", plane1, planeBytes);
        DumpTraceBytes(prefix + ".post.plane2.bin", plane2, planeBytes);
    }

    private static void InstallLargeCallsiteHooks(IntPtr library)
    {
        TraceLines.Clear();
        SourcePrepass4570CallCount = 0;
        PostGeometryPrepareCallCount = 0;
        PostGeometryStage4810CallCount = 0;
        PostGeometryCenterScaleCallCount = 0;
        PostGeometryStage2DD0CallCount = 0;
        PostGeometryStage3890CallCount = 0;
        PostGeometryStage3060CallCount = 0;
        PostGeometryDualScaleCallCount = 0;
        PostRgbStage42A0CallCount = 0;
        PostRgbStage3B00CallCount = 0;
        PostRgbStage40F0CallCount = 0;

        SourcePrepass4570HookDelegate = HookSourcePrepass4570;
        PostGeometryPrepareHookDelegate = HookPostGeometryPrepare;
        PostGeometryStage4810HookDelegate = HookPostGeometryStage4810;
        PostGeometryCenterScaleHookDelegate = HookPostGeometryCenterScale;
        PostGeometryStage2DD0HookDelegate = HookPostGeometryStage2DD0;
        PostGeometryStage3890HookDelegate = HookPostGeometryStage3890;
        PostGeometryStage3060HookDelegate = HookPostGeometryStage3060;
        PostGeometryDualScaleHookDelegate = HookPostGeometryDualScale;
        PostRgbStage42A0HookDelegate = HookPostRgbStage42A0;
        PostRgbStage3B00HookDelegate = HookPostRgbStage3B00;
        PostRgbStage40F0HookDelegate = HookPostRgbStage40F0;

        SourcePrepass4570Hook = InstallHook(
            IntPtr.Add(library, 0x4570),
            Marshal.GetFunctionPointerForDelegate(SourcePrepass4570HookDelegate),
            9);
        SourcePrepass4570Trampoline =
            (InternalSourcePrepass4570Delegate)Marshal.GetDelegateForFunctionPointer(
                SourcePrepass4570Hook.Trampoline,
                typeof(InternalSourcePrepass4570Delegate));

        PostGeometryPrepareHook = InstallHook(
            IntPtr.Add(library, 0x2410),
            Marshal.GetFunctionPointerForDelegate(PostGeometryPrepareHookDelegate),
            7);
        PostGeometryPrepareTrampoline =
            (InternalPostGeometryPrepareDelegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryPrepareHook.Trampoline,
                typeof(InternalPostGeometryPrepareDelegate));

        PostGeometryStage4810Hook = InstallHook(
            IntPtr.Add(library, 0x4810),
            Marshal.GetFunctionPointerForDelegate(PostGeometryStage4810HookDelegate),
            7);
        PostGeometryStage4810Trampoline =
            (InternalPostGeometryStage4810Delegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryStage4810Hook.Trampoline,
                typeof(InternalPostGeometryStage4810Delegate));

        PostGeometryCenterScaleHook = InstallHook(
            IntPtr.Add(library, 0x2C40),
            Marshal.GetFunctionPointerForDelegate(PostGeometryCenterScaleHookDelegate),
            7);
        PostGeometryCenterScaleTrampoline =
            (InternalPostGeometryCenterScaleDelegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryCenterScaleHook.Trampoline,
                typeof(InternalPostGeometryCenterScaleDelegate));

        PostGeometryStage2DD0Hook = InstallHook(
            IntPtr.Add(library, 0x2DD0),
            Marshal.GetFunctionPointerForDelegate(PostGeometryStage2DD0HookDelegate),
            7);
        PostGeometryStage2DD0Trampoline =
            (InternalPostGeometryStage2DD0Delegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryStage2DD0Hook.Trampoline,
                typeof(InternalPostGeometryStage2DD0Delegate));

        PostGeometryStage3890Hook = InstallHook(
            IntPtr.Add(library, 0x3890),
            Marshal.GetFunctionPointerForDelegate(PostGeometryStage3890HookDelegate),
            7);
        PostGeometryStage3890Trampoline =
            (InternalPostGeometryStage3890Delegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryStage3890Hook.Trampoline,
                typeof(InternalPostGeometryStage3890Delegate));

        PostGeometryStage3060Hook = InstallHook(
            IntPtr.Add(library, 0x3060),
            Marshal.GetFunctionPointerForDelegate(PostGeometryStage3060HookDelegate),
            7);
        PostGeometryStage3060Trampoline =
            (InternalPostGeometryStage3060Delegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryStage3060Hook.Trampoline,
                typeof(InternalPostGeometryStage3060Delegate));

        PostGeometryDualScaleHook = InstallHook(
            IntPtr.Add(library, 0x4450),
            Marshal.GetFunctionPointerForDelegate(PostGeometryDualScaleHookDelegate),
            10);
        PostGeometryDualScaleTrampoline =
            (InternalPostGeometryDualScaleDelegate)Marshal.GetDelegateForFunctionPointer(
                PostGeometryDualScaleHook.Trampoline,
                typeof(InternalPostGeometryDualScaleDelegate));

        PostRgbStage42A0Hook = InstallHook(
            IntPtr.Add(library, 0x42A0),
            Marshal.GetFunctionPointerForDelegate(PostRgbStage42A0HookDelegate),
            7);
        PostRgbStage42A0Trampoline =
            (InternalPostRgbStage42A0Delegate)Marshal.GetDelegateForFunctionPointer(
                PostRgbStage42A0Hook.Trampoline,
                typeof(InternalPostRgbStage42A0Delegate));

        PostRgbStage3B00Hook = InstallHook(
            IntPtr.Add(library, 0x3B00),
            Marshal.GetFunctionPointerForDelegate(PostRgbStage3B00HookDelegate),
            10);
        PostRgbStage3B00Trampoline =
            (InternalPostRgbStage3B00Delegate)Marshal.GetDelegateForFunctionPointer(
                PostRgbStage3B00Hook.Trampoline,
                typeof(InternalPostRgbStage3B00Delegate));

        PostRgbStage40F0Hook = InstallHook(
            IntPtr.Add(library, 0x40F0),
            Marshal.GetFunctionPointerForDelegate(PostRgbStage40F0HookDelegate),
            7);
        PostRgbStage40F0Trampoline =
            (InternalPostRgbStage40F0Delegate)Marshal.GetDelegateForFunctionPointer(
                PostRgbStage40F0Hook.Trampoline,
                typeof(InternalPostRgbStage40F0Delegate));
    }

    private static void RestoreLargeCallsiteHooks()
    {
        RestoreHook(PostRgbStage40F0Hook);
        RestoreHook(PostRgbStage3B00Hook);
        RestoreHook(PostRgbStage42A0Hook);
        RestoreHook(PostGeometryDualScaleHook);
        RestoreHook(PostGeometryStage3060Hook);
        RestoreHook(PostGeometryStage3890Hook);
        RestoreHook(PostGeometryStage2DD0Hook);
        RestoreHook(PostGeometryCenterScaleHook);
        RestoreHook(PostGeometryStage4810Hook);
        RestoreHook(PostGeometryPrepareHook);
        RestoreHook(SourcePrepass4570Hook);

        PostRgbStage40F0Hook = null;
        PostRgbStage3B00Hook = null;
        PostRgbStage42A0Hook = null;
        PostGeometryDualScaleHook = null;
        PostGeometryStage3060Hook = null;
        PostGeometryStage3890Hook = null;
        PostGeometryStage2DD0Hook = null;
        PostGeometryCenterScaleHook = null;
        PostGeometryStage4810Hook = null;
        PostGeometryPrepareHook = null;
        SourcePrepass4570Hook = null;
    }

    private static int Main(string[] args)
    {
        try
        {
            Options options = ParseArgs(args);
            if (options.DumpResampleLutWidth.HasValue)
            {
                return DumpResampleLut(options);
            }
            if (options.DumpGeometryStage)
            {
                return DumpGeometryStage(options);
            }
            if (options.DumpNormalizeStage)
            {
                return DumpNormalizeStage(options);
            }
            if (options.DumpSourceStage)
            {
                return DumpSourceStage(options);
            }
            if (options.DumpSourcePrepass4570)
            {
                return DumpSourcePrepass4570(options);
            }
            if (options.DumpPostGeometryPrepare)
            {
                return DumpPostGeometryPrepare(options);
            }
            if (options.DumpPostGeometryFilter)
            {
                return DumpPostGeometryFilter(options);
            }
            if (options.DumpPostGeometryCenterScale)
            {
                return DumpPostGeometryCenterScale(options);
            }
            if (options.DumpPostGeometryRgbConvert)
            {
                return DumpPostGeometryRgbConvert(options);
            }
            if (options.DumpPostGeometryEdgeResponse)
            {
                return DumpPostGeometryEdgeResponse(options);
            }
            if (options.DumpPostGeometryDualScale)
            {
                return DumpPostGeometryDualScale(options);
            }
            if (options.DumpPostGeometryStage2A00)
            {
                return DumpPostGeometryStage2A00(options);
            }
            if (options.DumpPostGeometryStage2DD0)
            {
                return DumpPostGeometryStage2DD0(options);
            }
            if (options.DumpPostGeometryStage4810)
            {
                return DumpPostGeometryStage4810(options);
            }
            if (options.DumpPostGeometryStage3600)
            {
                return DumpPostGeometryStage3600(options);
            }
            if (options.DumpPostGeometryStage3890)
            {
                return DumpPostGeometryStage3890(options);
            }
            if (options.DumpPostGeometryStage3060)
            {
                return DumpPostGeometryStage3060(options);
            }
            if (options.DumpPostRgbStage3B00)
            {
                return DumpPostRgbStage3B00(options);
            }
            if (options.DumpPostRgbStage3F40)
            {
                return DumpPostRgbStage3F40(options);
            }
            if (options.DumpPostRgbStage42A0)
            {
                return DumpPostRgbStage42A0(options);
            }
            if (options.DumpPostRgbStage40F0)
            {
                return DumpPostRgbStage40F0(options);
            }
            if (options.DumpRowResample)
            {
                return DumpRowResample(options);
            }
            if (options.TraceLargeCallsiteParams)
            {
                return TraceLargeCallsiteParams(options);
            }
            return Convert(options);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }

    private static int Convert(Options options)
    {
        byte[] dat = File.ReadAllBytes(options.InputPath);
        if (dat.Length != ExpectedDatSize)
        {
            throw new InvalidOperationException(
                "Expected a 0x20000-byte DAT file, got " + dat.Length + " bytes.");
        }

        byte[] trailer = new byte[TrailerSize];
        Array.Copy(dat, RawBlockSize, trailer, 0, TrailerSize);
        ValidateTrailer(trailer, options.ForceUnsignedTrailer);

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr proc = GetProcAddress(library, "DJ1000GraphicConv");
        if (proc == IntPtr.Zero)
        {
            FreeLibrary(library);
            throw new InvalidOperationException("GetProcAddress failed for DJ1000GraphicConv.");
        }

        Dj1000GraphicConvDelegate converter =
            (Dj1000GraphicConvDelegate)Marshal.GetDelegateForFunctionPointer(
                proc,
                typeof(Dj1000GraphicConvDelegate));

        byte[] buffer = new byte[WorkingBufferSize];
        Array.Copy(dat, buffer, dat.Length);

        int[] settingsBlock = BuildSettingsBlock(options, trailer);

        IntPtr imageBuffer = Marshal.AllocHGlobal(buffer.Length);
        IntPtr settings = Marshal.AllocHGlobal(settingsBlock.Length * 4);

        try
        {
            Marshal.Copy(buffer, 0, imageBuffer, buffer.Length);
            Marshal.Copy(settingsBlock, 0, settings, settingsBlock.Length);

            int result = converter(options.ConverterMode, imageBuffer, settings);
            if (result == 0)
            {
                throw new InvalidOperationException("DJ1000GraphicConv returned 0.");
            }

            byte[] dibHeader = new byte[40];
            Marshal.Copy(imageBuffer, dibHeader, 0, dibHeader.Length);
            WriteBitmap(imageBuffer, dibHeader, options.OutputPath);

            int width = ReadInt32(dibHeader, 4);
            int height = ReadInt32(dibHeader, 8);
            int bitCount = ReadInt16(dibHeader, 14);
            Console.WriteLine(
                "converted " + Path.GetFileName(options.InputPath) + " -> " +
                Path.GetFileName(options.OutputPath) + " (" + width + "x" + height +
                ", " + bitCount + " bpp, export_mode=" + options.ExportMode + ")");
        }
        finally
        {
            Marshal.FreeHGlobal(settings);
            Marshal.FreeHGlobal(imageBuffer);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int TraceLargeCallsiteParams(Options options)
    {
        byte[] dat = File.ReadAllBytes(options.InputPath);
        if (dat.Length != ExpectedDatSize)
        {
            throw new InvalidOperationException(
                "Expected a 0x20000-byte DAT file, got " + dat.Length + " bytes.");
        }

        byte[] trailer = new byte[TrailerSize];
        Array.Copy(dat, RawBlockSize, trailer, 0, TrailerSize);
        ValidateTrailer(trailer, options.ForceUnsignedTrailer);

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr proc = GetProcAddress(library, "DJ1000GraphicConv");
        if (proc == IntPtr.Zero)
        {
            FreeLibrary(library);
            throw new InvalidOperationException("GetProcAddress failed for DJ1000GraphicConv.");
        }

        Dj1000GraphicConvDelegate converter =
            (Dj1000GraphicConvDelegate)Marshal.GetDelegateForFunctionPointer(
                proc,
                typeof(Dj1000GraphicConvDelegate));

        byte[] buffer = new byte[WorkingBufferSize];
        Array.Copy(dat, buffer, dat.Length);
        int[] settingsBlock = BuildSettingsBlock(options, trailer);

        IntPtr imageBuffer = Marshal.AllocHGlobal(buffer.Length);
        IntPtr settings = Marshal.AllocHGlobal(settingsBlock.Length * 4);

        try
        {
            Marshal.Copy(buffer, 0, imageBuffer, buffer.Length);
            Marshal.Copy(settingsBlock, 0, settings, settingsBlock.Length);
            TraceOutputPrefix = options.OutputPath;

            InstallLargeCallsiteHooks(library);
            try
            {
                TraceLine(
                    "settings=" +
                    string.Join(",", settingsBlock));
                int result = converter(options.ConverterMode, imageBuffer, settings);
                if (result == 0)
                {
                    throw new InvalidOperationException("DJ1000GraphicConv returned 0.");
                }
            }
            finally
            {
                RestoreLargeCallsiteHooks();
                TraceOutputPrefix = string.Empty;
            }

            byte[] dibHeader = new byte[40];
            Marshal.Copy(imageBuffer, dibHeader, 0, dibHeader.Length);
            int width = ReadInt32(dibHeader, 4);
            int height = ReadInt32(dibHeader, 8);
            int bitCount = ReadInt16(dibHeader, 14);
            int stride = ((width * (bitCount / 8)) + 3) & ~3;
            int pixelBytes = stride * Math.Abs(height);
            byte[] pixelData = new byte[pixelBytes];
            Marshal.Copy(IntPtr.Add(imageBuffer, 40), pixelData, 0, pixelBytes);
            File.WriteAllBytes(options.OutputPath + ".final.pixeldata.bin", pixelData);
            WriteBitmap(imageBuffer, dibHeader, options.OutputPath);

            string tracePath = options.OutputPath + ".trace.txt";
            File.WriteAllLines(tracePath, TraceLines.ToArray());

            Console.WriteLine(
                "traced " + Path.GetFileName(options.InputPath) + " -> " +
                Path.GetFileName(options.OutputPath) + " (" + width + "x" + height +
                ", " + bitCount + " bpp, export_mode=" + options.ExportMode + ")" +
                " trace=" + tracePath);
        }
        finally
        {
            Marshal.FreeHGlobal(settings);
            Marshal.FreeHGlobal(imageBuffer);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpResampleLut(Options options)
    {
        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr output = Marshal.AllocHGlobal(0x320);
        try
        {
            IntPtr proc = IntPtr.Add(library, 0x1210);
            InternalResampleLutDelegate dumper =
                (InternalResampleLutDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalResampleLutDelegate));

            Marshal.WriteInt32(IntPtr.Add(library, 0x1A0E4), options.HelperModeE4);
            for (int index = 0; index < 0x320; index++)
            {
                Marshal.WriteByte(output, index, 0);
            }

            dumper(output, options.DumpResampleLutWidth.Value, 0);

            int tableLength = Marshal.ReadInt32(IntPtr.Add(library, 0x1C1BC));
            if (tableLength < 0 || tableLength > 0x320)
            {
                throw new InvalidOperationException(
                    "Unexpected table length from internal helper: " + tableLength + ".");
            }

            byte[] packed = new byte[tableLength];
            Marshal.Copy(output, packed, 0, tableLength);

            string directory = Path.GetDirectoryName(options.OutputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(options.OutputPath, packed);
            Console.WriteLine(
                "dumped resample_lut width=" + options.DumpResampleLutWidth.Value +
                " length=" + tableLength +
                " mode_e4=" + options.HelperModeE4 +
                " -> " + Path.GetFileName(options.OutputPath));
        }
        finally
        {
            Marshal.FreeHGlobal(output);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpGeometryStage(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Geometry-stage dumping requires --plane0, --plane1, and --plane2.");
        }

        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Bytes = File.ReadAllBytes(options.Plane2Path);
        const int planeBytes = 0x2F400;
        if (plane0Bytes.Length != planeBytes ||
            plane1Bytes.Length != planeBytes ||
            plane2Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Geometry-stage inputs must each be exactly 0x2F400 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, planeBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, planeBytes);
            Marshal.Copy(plane2Bytes, 0, plane2, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x13C0);
            InternalGeometryStageDelegate stage =
                (InternalGeometryStageDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalGeometryStageDelegate));

            int result = stage(
                options.GeometryPreviewFlag,
                options.GeometryExportMode,
                options.GeometrySelector,
                plane0,
                plane1,
                plane2);
            if (result < 0)
            {
                throw new InvalidOperationException(
                    "Internal geometry stage returned " + result + ".");
            }

            byte[] out0 = new byte[planeBytes];
            byte[] out1 = new byte[planeBytes];
            byte[] out2 = new byte[planeBytes];
            Marshal.Copy(plane0, out0, 0, planeBytes);
            Marshal.Copy(plane1, out1, 0, planeBytes);
            Marshal.Copy(plane2, out2, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.bin", out0);
            File.WriteAllBytes(outputPrefix + ".plane1.bin", out1);
            File.WriteAllBytes(outputPrefix + ".plane2.bin", out2);
            Console.WriteLine(
                "dumped geometry stage preview=" + options.GeometryPreviewFlag +
                " export_mode=" + options.GeometryExportMode +
                " selector=" + options.GeometrySelector +
                " -> " + outputPrefix + ".plane{0,1,2}.bin");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpNormalizeStage(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Normalize-stage dumping requires --plane0, --plane1, and --plane2.");
        }

        int defaultStride = options.NormalizePreviewFlag == 0 ? 0x200 : 0x80;
        int defaultRows = options.NormalizePreviewFlag == 0 ? 0x0F6 : 0x40;
        int stride = options.NormalizeStride > 0 ? options.NormalizeStride : defaultStride;
        int rows = options.NormalizeRows > 0 ? options.NormalizeRows : defaultRows;
        int planeBytes = stride * rows * 4;

        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Bytes = File.ReadAllBytes(options.Plane2Path);
        if (plane0Bytes.Length != planeBytes ||
            plane1Bytes.Length != planeBytes ||
            plane2Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Normalize-stage inputs must each match the configured float-plane size.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, planeBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, planeBytes);
            Marshal.Copy(plane2Bytes, 0, plane2, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x1E00);
            InternalNormalizeStageDelegate stage =
                (InternalNormalizeStageDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalNormalizeStageDelegate));

            stage(options.NormalizePreviewFlag, plane0, plane1, plane2);

            byte[] out0 = new byte[planeBytes];
            byte[] out1 = new byte[planeBytes];
            byte[] out2 = new byte[planeBytes];
            Marshal.Copy(plane0, out0, 0, planeBytes);
            Marshal.Copy(plane1, out1, 0, planeBytes);
            Marshal.Copy(plane2, out2, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.f32", out0);
            File.WriteAllBytes(outputPrefix + ".plane1.f32", out1);
            File.WriteAllBytes(outputPrefix + ".plane2.f32", out2);
            Console.WriteLine(
                "dumped normalize stage preview=" + options.NormalizePreviewFlag +
                " stride=" + stride +
                " rows=" + rows +
                " floats=" + (planeBytes / 4) +
                " -> " + outputPrefix + ".plane{0,1,2}.f32");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpSourceStage(Options options)
    {
        if (string.IsNullOrEmpty(options.SourceInputPath))
        {
            throw new InvalidOperationException("Source-stage dumping requires --source-input.");
        }

        int previewFlag = options.SourcePreviewFlag;
        int inputStride = previewFlag == 0 ? 0x200 : 0x80;
        int inputRows = previewFlag == 0 ? 0x0F4 : 0x40;
        int defaultOutputStride = inputStride;
        int defaultOutputRows = previewFlag == 0 ? 0x0F6 : 0x40;
        int outputStride =
            options.SourceOutputStride > 0 ? options.SourceOutputStride : defaultOutputStride;
        int outputRows =
            options.SourceOutputRows > 0 ? options.SourceOutputRows : defaultOutputRows;
        int inputBytes = inputStride * inputRows;
        int outputPlaneBytes = outputStride * outputRows * 4;

        byte[] sourceBytes = File.ReadAllBytes(options.SourceInputPath);
        if (sourceBytes.Length < inputBytes)
        {
            throw new InvalidOperationException(
                "Source-stage input must be at least " + inputBytes + " bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr source = Marshal.AllocHGlobal(inputBytes);
        IntPtr plane0 = Marshal.AllocHGlobal(outputPlaneBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(outputPlaneBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(outputPlaneBytes);

        try
        {
            Marshal.Copy(sourceBytes, 0, source, inputBytes);
            for (int index = 0; index < outputPlaneBytes; index++)
            {
                Marshal.WriteByte(plane0, index, 0);
                Marshal.WriteByte(plane1, index, 0);
                Marshal.WriteByte(plane2, index, 0);
            }

            IntPtr proc = IntPtr.Add(library, 0x5EB0);
            InternalSourceStageDelegate stage =
                (InternalSourceStageDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalSourceStageDelegate));

            stage(previewFlag, plane0, plane1, plane2, source);

            byte[] out0 = new byte[outputPlaneBytes];
            byte[] out1 = new byte[outputPlaneBytes];
            byte[] out2 = new byte[outputPlaneBytes];
            Marshal.Copy(plane0, out0, 0, outputPlaneBytes);
            Marshal.Copy(plane1, out1, 0, outputPlaneBytes);
            Marshal.Copy(plane2, out2, 0, outputPlaneBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.f32", out0);
            File.WriteAllBytes(outputPrefix + ".plane1.f32", out1);
            File.WriteAllBytes(outputPrefix + ".plane2.f32", out2);
            Console.WriteLine(
                "dumped source stage preview=" + previewFlag +
                " input_bytes=" + inputBytes +
                " output_stride=" + outputStride +
                " output_rows=" + outputRows +
                " output_floats=" + (outputPlaneBytes / 4) +
                " -> " + outputPrefix + ".plane{0,1,2}.f32");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            Marshal.FreeHGlobal(source);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpSourcePrepass4570(Options options)
    {
        if (string.IsNullOrEmpty(options.SourceInputPath))
        {
            throw new InvalidOperationException(
                "Source prepass 0x4570 dumping requires --source-input.");
        }

        int previewFlag = options.SourcePreviewFlag;
        int inputStride = previewFlag == 0 ? 0x200 : 0x80;
        int inputRows = previewFlag == 0 ? 0x0F4 : 0x40;
        int inputBytes = inputStride * inputRows;

        byte[] sourceBytes = File.ReadAllBytes(options.SourceInputPath);
        if (sourceBytes.Length < inputBytes)
        {
            throw new InvalidOperationException(
                "Source prepass 0x4570 input must be at least " + inputBytes + " bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane = Marshal.AllocHGlobal(inputBytes);

        try
        {
            Marshal.Copy(sourceBytes, 0, plane, inputBytes);

            IntPtr proc = IntPtr.Add(library, 0x4570);
            InternalSourcePrepass4570Delegate stage =
                (InternalSourcePrepass4570Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalSourcePrepass4570Delegate));

            int result = stage(previewFlag, plane);
            if (result == 0)
            {
                throw new InvalidOperationException("Internal source prepass 0x4570 returned 0.");
            }

            byte[] outputBuffer = new byte[inputBytes];
            Marshal.Copy(plane, outputBuffer, 0, inputBytes);

            string outputPath = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPath, outputBuffer);
            Console.WriteLine(
                "dumped source prepass 0x4570 preview=" + previewFlag +
                " input_bytes=" + inputBytes +
                " -> " + outputPath);
        }
        finally
        {
            Marshal.FreeHGlobal(plane);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryPrepare(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-geometry prepare dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry prepare dumping requires positive --stage-width and --stage-height.");
        }

        int inputBytes = options.StageWidth * options.StageHeight;
        int outputBytes = inputBytes * 8;

        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Bytes = File.ReadAllBytes(options.Plane2Path);
        if (plane0Bytes.Length < inputBytes ||
            plane1Bytes.Length < inputBytes ||
            plane2Bytes.Length < inputBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry prepare inputs must each contain at least width*height bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(inputBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(inputBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(inputBytes);
        IntPtr delta0 = Marshal.AllocHGlobal(outputBytes);
        IntPtr center = Marshal.AllocHGlobal(outputBytes);
        IntPtr delta2 = Marshal.AllocHGlobal(outputBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, inputBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, inputBytes);
            Marshal.Copy(plane2Bytes, 0, plane2, inputBytes);
            ZeroMemory(delta0, outputBytes);
            ZeroMemory(center, outputBytes);
            ZeroMemory(delta2, outputBytes);

            IntPtr proc = IntPtr.Add(library, 0x2410);
            InternalPostGeometryPrepareDelegate stage =
                (InternalPostGeometryPrepareDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryPrepareDelegate));

            stage(
                options.StageWidth,
                options.StageHeight,
                plane0,
                plane1,
                plane2,
                delta0,
                center,
                delta2);

            byte[] centerBytes = new byte[outputBytes];
            byte[] delta0Bytes = new byte[outputBytes];
            byte[] delta2Bytes = new byte[outputBytes];
            Marshal.Copy(center, centerBytes, 0, outputBytes);
            Marshal.Copy(delta0, delta0Bytes, 0, outputBytes);
            Marshal.Copy(delta2, delta2Bytes, 0, outputBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".center.f64", centerBytes);
            File.WriteAllBytes(outputPrefix + ".delta0.f64", delta0Bytes);
            File.WriteAllBytes(outputPrefix + ".delta2.f64", delta2Bytes);
            Console.WriteLine(
                "dumped post-geometry prepare width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".{center,delta0,delta2}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(delta2);
            Marshal.FreeHGlobal(center);
            Marshal.FreeHGlobal(delta0);
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryFilter(Options options)
    {
        if (string.IsNullOrEmpty(options.Delta0Path) ||
            string.IsNullOrEmpty(options.Delta2Path))
        {
            throw new InvalidOperationException(
                "Post-geometry filter dumping requires --delta0 and --delta2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry filter dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] delta0Bytes = File.ReadAllBytes(options.Delta0Path);
        byte[] delta2Bytes = File.ReadAllBytes(options.Delta2Path);
        if (delta0Bytes.Length != planeBytes || delta2Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry filter inputs must each be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr delta0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr delta2 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(delta0Bytes, 0, delta0, planeBytes);
            Marshal.Copy(delta2Bytes, 0, delta2, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x21A0);
            InternalPostGeometryFilterDelegate stage =
                (InternalPostGeometryFilterDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryFilterDelegate));

            int result = stage(options.StageWidth, options.StageHeight, delta0, delta2);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry filter returned 0.");
            }

            byte[] outDelta0 = new byte[planeBytes];
            byte[] outDelta2 = new byte[planeBytes];
            Marshal.Copy(delta0, outDelta0, 0, planeBytes);
            Marshal.Copy(delta2, outDelta2, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".delta0.f64", outDelta0);
            File.WriteAllBytes(outputPrefix + ".delta2.f64", outDelta2);
            Console.WriteLine(
                "dumped post-geometry filter width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".{delta0,delta2}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(delta2);
            Marshal.FreeHGlobal(delta0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryCenterScale(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-geometry center-scale dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry center-scale dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Bytes = File.ReadAllBytes(options.Plane2Path);
        if (plane0Bytes.Length != planeBytes ||
            plane1Bytes.Length != planeBytes ||
            plane2Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry center-scale inputs must each be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, planeBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, planeBytes);
            Marshal.Copy(plane2Bytes, 0, plane2, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x2C40);
            InternalPostGeometryCenterScaleDelegate stage =
                (InternalPostGeometryCenterScaleDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryCenterScaleDelegate));

            stage(options.StageWidth, options.StageHeight, plane0, plane1, plane2);

            byte[] outPlane0 = new byte[planeBytes];
            byte[] outPlane1 = new byte[planeBytes];
            byte[] outPlane2 = new byte[planeBytes];
            Marshal.Copy(plane0, outPlane0, 0, planeBytes);
            Marshal.Copy(plane1, outPlane1, 0, planeBytes);
            Marshal.Copy(plane2, outPlane2, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".delta0.f64", outPlane0);
            File.WriteAllBytes(outputPrefix + ".center.f64", outPlane1);
            File.WriteAllBytes(outputPrefix + ".delta2.f64", outPlane2);
            Console.WriteLine(
                "dumped post-geometry center-scale width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".{delta0,center,delta2}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryRgbConvert(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-geometry RGB dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry RGB dumping requires positive --stage-width and --stage-height.");
        }

        int inputBytes = options.StageWidth * options.StageHeight * 8;
        int outputBytes = options.StageWidth * options.StageHeight;
        byte[] input0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] input1Bytes = File.ReadAllBytes(options.Plane1Path);
        byte[] input2Bytes = File.ReadAllBytes(options.Plane2Path);
        if (input0Bytes.Length < inputBytes ||
            input1Bytes.Length < inputBytes ||
            input2Bytes.Length < inputBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry RGB inputs must each contain at least width*height*8 bytes.");
        }
        int input0Length = input0Bytes.Length;
        int input1Length = input1Bytes.Length;
        int input2Length = input2Bytes.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr input0 = Marshal.AllocHGlobal(input0Length);
        IntPtr input1 = Marshal.AllocHGlobal(input1Length);
        IntPtr input2 = Marshal.AllocHGlobal(input2Length);
        IntPtr plane0 = Marshal.AllocHGlobal(outputBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(outputBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(outputBytes);

        try
        {
            Marshal.Copy(input0Bytes, 0, input0, input0Length);
            Marshal.Copy(input1Bytes, 0, input1, input1Length);
            Marshal.Copy(input2Bytes, 0, input2, input2Length);
            ZeroMemory(plane0, outputBytes);
            ZeroMemory(plane1, outputBytes);
            ZeroMemory(plane2, outputBytes);

            IntPtr proc = IntPtr.Add(library, 0x2530);
            InternalPostGeometryRgbConvertDelegate stage =
                (InternalPostGeometryRgbConvertDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryRgbConvertDelegate));

            stage(
                options.StageWidth,
                options.StageHeight,
                plane0,
                plane1,
                plane2,
                input0,
                input1,
                input2);

            byte[] outPlane0 = new byte[outputBytes];
            byte[] outPlane1 = new byte[outputBytes];
            byte[] outPlane2 = new byte[outputBytes];
            Marshal.Copy(plane0, outPlane0, 0, outputBytes);
            Marshal.Copy(plane1, outPlane1, 0, outputBytes);
            Marshal.Copy(plane2, outPlane2, 0, outputBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.bin", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.bin", outPlane1);
            File.WriteAllBytes(outputPrefix + ".plane2.bin", outPlane2);
            Console.WriteLine(
                "dumped post-geometry RGB width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".plane{0,1,2}.bin");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            Marshal.FreeHGlobal(input2);
            Marshal.FreeHGlobal(input1);
            Marshal.FreeHGlobal(input0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryEdgeResponse(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path))
        {
            throw new InvalidOperationException(
                "Post-geometry edge-response dumping requires --plane0.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry edge-response dumping requires positive --stage-width and --stage-height.");
        }

        int inputBytes = options.StageWidth * options.StageHeight * 8;
        int outputBytes = options.StageWidth * options.StageHeight * 4;
        byte[] inputBytesBuffer = File.ReadAllBytes(options.Plane0Path);
        if (inputBytesBuffer.Length != inputBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry edge-response input must be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr input = Marshal.AllocHGlobal(inputBytes);
        IntPtr output = Marshal.AllocHGlobal(outputBytes);

        try
        {
            Marshal.Copy(inputBytesBuffer, 0, input, inputBytes);
            ZeroMemory(output, outputBytes);

            IntPtr proc = IntPtr.Add(library, 0x2730);
            InternalPostGeometryEdgeResponseDelegate stage =
                (InternalPostGeometryEdgeResponseDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryEdgeResponseDelegate));

            int result = stage(options.StageWidth, options.StageHeight, output, input);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry edge-response returned 0.");
            }

            byte[] outputBuffer = new byte[outputBytes];
            Marshal.Copy(output, outputBuffer, 0, outputBytes);

            string outputPath = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPath, outputBuffer);
            Console.WriteLine(
                "dumped post-geometry edge-response width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPath);
        }
        finally
        {
            Marshal.FreeHGlobal(output);
            Marshal.FreeHGlobal(input);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryDualScale(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path))
        {
            throw new InvalidOperationException(
                "Post-geometry dual-scale dumping requires --plane0 and --plane1.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry dual-scale dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        if (plane0Bytes.Length != planeBytes || plane1Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry dual-scale inputs must each be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, planeBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x4450);
            InternalPostGeometryDualScaleDelegate stage =
                (InternalPostGeometryDualScaleDelegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryDualScaleDelegate));

            stage(options.StageWidth, options.StageHeight, plane0, plane1, options.Scalar);

            byte[] outPlane0 = new byte[planeBytes];
            byte[] outPlane1 = new byte[planeBytes];
            Marshal.Copy(plane0, outPlane0, 0, planeBytes);
            Marshal.Copy(plane1, outPlane1, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.f64", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.f64", outPlane1);
            Console.WriteLine(
                "dumped post-geometry dual-scale width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " scalar=" + options.Scalar.ToString(System.Globalization.CultureInfo.InvariantCulture) +
                " -> " + outputPrefix + ".{plane0,plane1}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryStage2A00(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path))
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x2a00 dumping requires --plane0.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x2a00 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] planeBytesBuffer = File.ReadAllBytes(options.Plane0Path);
        if (planeBytesBuffer.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x2a00 input must contain at least width*height*8 bytes.");
        }
        int inputBytes = planeBytesBuffer.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane = Marshal.AllocHGlobal(inputBytes);

        try
        {
            Marshal.Copy(planeBytesBuffer, 0, plane, inputBytes);

            IntPtr proc = IntPtr.Add(library, 0x2A00);
            InternalPostGeometryStage2A00Delegate stage =
                (InternalPostGeometryStage2A00Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryStage2A00Delegate));

            int result = stage(options.StageWidth, options.StageHeight, IntPtr.Zero, plane, IntPtr.Zero);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry stage 0x2a00 returned 0.");
            }

            byte[] outPlane = new byte[inputBytes];
            Marshal.Copy(plane, outPlane, 0, inputBytes);

            string outputPath = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPath, outPlane);
            Console.WriteLine(
                "dumped post-geometry stage 0x2a00 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPath);
        }
        finally
        {
            Marshal.FreeHGlobal(plane);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryStage2DD0(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path))
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x2dd0 dumping requires --plane0 and --plane1.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x2dd0 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        if (plane0Bytes.Length != planeBytes || plane1Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x2dd0 inputs must each be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, planeBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x2DD0);
            InternalPostGeometryStage2DD0Delegate stage =
                (InternalPostGeometryStage2DD0Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryStage2DD0Delegate));

            int result = stage(options.StageWidth, options.StageHeight, plane0, plane1);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry stage 0x2dd0 returned 0.");
            }

            byte[] outPlane0 = new byte[planeBytes];
            byte[] outPlane1 = new byte[planeBytes];
            Marshal.Copy(plane0, outPlane0, 0, planeBytes);
            Marshal.Copy(plane1, outPlane1, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.f64", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.f64", outPlane1);
            Console.WriteLine(
                "dumped post-geometry stage 0x2dd0 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".{plane0,plane1}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryStage4810(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x4810 dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x4810 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Bytes = File.ReadAllBytes(options.Plane2Path);
        if (plane0Bytes.Length < planeBytes ||
            plane1Bytes.Length < planeBytes ||
            plane2Bytes.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x4810 inputs must each contain at least width*height*8 bytes.");
        }
        int plane0InputBytes = plane0Bytes.Length;
        int plane1InputBytes = plane1Bytes.Length;
        int plane2InputBytes = plane2Bytes.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(plane0InputBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(plane1InputBytes);
        IntPtr plane2 = Marshal.AllocHGlobal(plane2InputBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, plane0InputBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, plane1InputBytes);
            Marshal.Copy(plane2Bytes, 0, plane2, plane2InputBytes);

            IntPtr proc = IntPtr.Add(library, 0x4810);
            InternalPostGeometryStage4810Delegate stage =
                (InternalPostGeometryStage4810Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryStage4810Delegate));

            int result = stage(options.StageWidth, options.StageHeight, plane0, plane1, plane2);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry stage 0x4810 returned 0.");
            }

            byte[] outPlane0 = new byte[plane0InputBytes];
            byte[] outPlane1 = new byte[plane1InputBytes];
            byte[] outPlane2 = new byte[plane2InputBytes];
            Marshal.Copy(plane0, outPlane0, 0, plane0InputBytes);
            Marshal.Copy(plane1, outPlane1, 0, plane1InputBytes);
            Marshal.Copy(plane2, outPlane2, 0, plane2InputBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.f64", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.f64", outPlane1);
            File.WriteAllBytes(outputPrefix + ".plane2.f64", outPlane2);
            Console.WriteLine(
                "dumped post-geometry stage 0x4810 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".{plane0,plane1,plane2}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryStage3600(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path))
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3600 dumping requires --plane0.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3600 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] planeBytesIn = File.ReadAllBytes(options.Plane0Path);
        if (planeBytesIn.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3600 input must contain at least width*height*8 bytes.");
        }
        int inputBytes = planeBytesIn.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane = Marshal.AllocHGlobal(inputBytes);

        try
        {
            Marshal.Copy(planeBytesIn, 0, plane, inputBytes);

            IntPtr proc = IntPtr.Add(library, 0x3600);
            InternalPostGeometryStage3600Delegate stage =
                (InternalPostGeometryStage3600Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryStage3600Delegate));

            int result = stage(options.StageWidth, options.StageHeight, plane);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry stage 0x3600 returned 0.");
            }

            byte[] outPlane = new byte[inputBytes];
            Marshal.Copy(plane, outPlane, 0, inputBytes);

            string outputPath = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPath, outPlane);
            Console.WriteLine(
                "dumped post-geometry stage 0x3600 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPath);
        }
        finally
        {
            Marshal.FreeHGlobal(plane);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryStage3890(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path))
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3890 dumping requires --plane0 and --plane1.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3890 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight * 8;
        byte[] plane0Bytes = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Bytes = File.ReadAllBytes(options.Plane1Path);
        if (plane0Bytes.Length != planeBytes || plane1Bytes.Length != planeBytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3890 inputs must each be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(planeBytes);
        IntPtr plane1 = Marshal.AllocHGlobal(planeBytes);

        try
        {
            Marshal.Copy(plane0Bytes, 0, plane0, planeBytes);
            Marshal.Copy(plane1Bytes, 0, plane1, planeBytes);

            IntPtr proc = IntPtr.Add(library, 0x3890);
            InternalPostGeometryStage3890Delegate stage =
                (InternalPostGeometryStage3890Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryStage3890Delegate));

            stage(options.StageWidth, options.StageHeight, options.Level, plane0, plane1);

            byte[] outPlane0 = new byte[planeBytes];
            byte[] outPlane1 = new byte[planeBytes];
            Marshal.Copy(plane0, outPlane0, 0, planeBytes);
            Marshal.Copy(plane1, outPlane1, 0, planeBytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.f64", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.f64", outPlane1);
            Console.WriteLine(
                "dumped post-geometry stage 0x3890 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " level=" + options.Level +
                " -> " + outputPrefix + ".{plane0,plane1}.f64");
        }
        finally
        {
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostGeometryStage3060(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path))
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3060 dumping requires --plane0 and --plane1.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3060 dumping requires positive --stage-width and --stage-height.");
        }

        int plane0Bytes = options.StageWidth * options.StageHeight * 4;
        int plane1Bytes = options.StageWidth * options.StageHeight * 8;
        byte[] plane0Input = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Input = File.ReadAllBytes(options.Plane1Path);
        if (plane0Input.Length != plane0Bytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3060 plane0 input must be exactly width*height*4 bytes.");
        }
        if (plane1Input.Length != plane1Bytes)
        {
            throw new InvalidOperationException(
                "Post-geometry stage 0x3060 plane1 input must be exactly width*height*8 bytes.");
        }

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(plane0Bytes);
        IntPtr plane1 = Marshal.AllocHGlobal(plane1Bytes);

        try
        {
            Marshal.Copy(plane0Input, 0, plane0, plane0Bytes);
            Marshal.Copy(plane1Input, 0, plane1, plane1Bytes);

            IntPtr proc = IntPtr.Add(library, 0x3060);
            InternalPostGeometryStage3060Delegate stage =
                (InternalPostGeometryStage3060Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostGeometryStage3060Delegate));

            int result = stage(
                options.StageWidth,
                options.StageHeight,
                options.StageParam0,
                options.StageParam1,
                0xA0,
                0x0A,
                plane0,
                options.Scalar,
                0,
                plane1,
                options.Threshold);
            if (result == 0)
            {
                throw new InvalidOperationException(
                    "Internal post-geometry stage 0x3060 returned 0.");
            }

            byte[] outPlane = new byte[plane1Bytes];
            Marshal.Copy(plane1, outPlane, 0, plane1Bytes);

            string outputPath = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPath, outPlane);
            Console.WriteLine(
                "dumped post-geometry stage 0x3060 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " stage_param0=" + options.StageParam0 +
                " stage_param1=" + options.StageParam1 +
                " scalar=" + options.Scalar.ToString(System.Globalization.CultureInfo.InvariantCulture) +
                " threshold=" + options.Threshold +
                " -> " + outputPath);
        }
        finally
        {
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostRgbStage3B00(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x3b00 dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x3b00 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight;
        byte[] plane0Input = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Input = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Input = File.ReadAllBytes(options.Plane2Path);
        if (plane0Input.Length < planeBytes ||
            plane1Input.Length < planeBytes ||
            plane2Input.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x3b00 inputs must each contain at least width*height bytes.");
        }
        int plane0Bytes = plane0Input.Length;
        int plane1Bytes = plane1Input.Length;
        int plane2Bytes = plane2Input.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(plane0Bytes);
        IntPtr plane1 = Marshal.AllocHGlobal(plane1Bytes);
        IntPtr plane2 = Marshal.AllocHGlobal(plane2Bytes);

        try
        {
            Marshal.Copy(plane0Input, 0, plane0, plane0Bytes);
            Marshal.Copy(plane1Input, 0, plane1, plane1Bytes);
            Marshal.Copy(plane2Input, 0, plane2, plane2Bytes);

            IntPtr proc = IntPtr.Add(library, 0x3B00);
            InternalPostRgbStage3B00Delegate stage =
                (InternalPostRgbStage3B00Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostRgbStage3B00Delegate));

            stage(
                options.StageWidth,
                options.StageHeight,
                options.Level,
                options.Scalar,
                plane0,
                plane1,
                plane2);

            byte[] outPlane0 = new byte[plane0Bytes];
            byte[] outPlane1 = new byte[plane1Bytes];
            byte[] outPlane2 = new byte[plane2Bytes];
            Marshal.Copy(plane0, outPlane0, 0, plane0Bytes);
            Marshal.Copy(plane1, outPlane1, 0, plane1Bytes);
            Marshal.Copy(plane2, outPlane2, 0, plane2Bytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.bin", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.bin", outPlane1);
            File.WriteAllBytes(outputPrefix + ".plane2.bin", outPlane2);
            Console.WriteLine(
                "dumped post-RGB stage 0x3b00 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " level=" + options.Level +
                " scalar=" + options.Scalar.ToString(System.Globalization.CultureInfo.InvariantCulture) +
                " -> " + outputPrefix + ".{plane0,plane1,plane2}.bin");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostRgbStage3F40(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x3f40 dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x3f40 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight;
        byte[] plane0Input = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Input = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Input = File.ReadAllBytes(options.Plane2Path);
        if (plane0Input.Length < planeBytes ||
            plane1Input.Length < planeBytes ||
            plane2Input.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x3f40 inputs must each contain at least width*height bytes.");
        }
        int plane0Bytes = plane0Input.Length;
        int plane1Bytes = plane1Input.Length;
        int plane2Bytes = plane2Input.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(plane0Bytes);
        IntPtr plane1 = Marshal.AllocHGlobal(plane1Bytes);
        IntPtr plane2 = Marshal.AllocHGlobal(plane2Bytes);

        try
        {
            Marshal.Copy(plane0Input, 0, plane0, plane0Bytes);
            Marshal.Copy(plane1Input, 0, plane1, plane1Bytes);
            Marshal.Copy(plane2Input, 0, plane2, plane2Bytes);

            IntPtr proc = IntPtr.Add(library, 0x3F40);
            InternalPostRgbStage3F40Delegate stage =
                (InternalPostRgbStage3F40Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostRgbStage3F40Delegate));

            stage(options.StageWidth, options.StageHeight, options.Level, plane0, plane1, plane2);

            byte[] outPlane0 = new byte[plane0Bytes];
            byte[] outPlane1 = new byte[plane1Bytes];
            byte[] outPlane2 = new byte[plane2Bytes];
            Marshal.Copy(plane0, outPlane0, 0, plane0Bytes);
            Marshal.Copy(plane1, outPlane1, 0, plane1Bytes);
            Marshal.Copy(plane2, outPlane2, 0, plane2Bytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.bin", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.bin", outPlane1);
            File.WriteAllBytes(outputPrefix + ".plane2.bin", outPlane2);
            Console.WriteLine(
                "dumped post-RGB stage 0x3f40 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " -> " + outputPrefix + ".plane{0,1,2}.bin");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostRgbStage42A0(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x42a0 dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x42a0 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight;
        byte[] plane0Input = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Input = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Input = File.ReadAllBytes(options.Plane2Path);
        if (plane0Input.Length < planeBytes ||
            plane1Input.Length < planeBytes ||
            plane2Input.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x42a0 inputs must each contain at least width*height bytes.");
        }
        int plane0Bytes = plane0Input.Length;
        int plane1Bytes = plane1Input.Length;
        int plane2Bytes = plane2Input.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(plane0Bytes);
        IntPtr plane1 = Marshal.AllocHGlobal(plane1Bytes);
        IntPtr plane2 = Marshal.AllocHGlobal(plane2Bytes);

        try
        {
            Marshal.Copy(plane0Input, 0, plane0, plane0Bytes);
            Marshal.Copy(plane1Input, 0, plane1, plane1Bytes);
            Marshal.Copy(plane2Input, 0, plane2, plane2Bytes);

            IntPtr proc = IntPtr.Add(library, 0x42A0);
            InternalPostRgbStage42A0Delegate stage =
                (InternalPostRgbStage42A0Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostRgbStage42A0Delegate));

            stage(
                options.StageWidth,
                options.StageHeight,
                plane0,
                plane1,
                plane2,
                options.StageParam0,
                options.StageParam1,
                options.StageParam2);

            byte[] outPlane0 = new byte[plane0Bytes];
            byte[] outPlane1 = new byte[plane1Bytes];
            byte[] outPlane2 = new byte[plane2Bytes];
            Marshal.Copy(plane0, outPlane0, 0, plane0Bytes);
            Marshal.Copy(plane1, outPlane1, 0, plane1Bytes);
            Marshal.Copy(plane2, outPlane2, 0, plane2Bytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.bin", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.bin", outPlane1);
            File.WriteAllBytes(outputPrefix + ".plane2.bin", outPlane2);
            Console.WriteLine(
                "dumped post-RGB stage 0x42a0 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " scale0=" + options.StageParam0 +
                " scale1=" + options.StageParam1 +
                " scale2=" + options.StageParam2 +
                " -> " + outputPrefix + ".{plane0,plane1,plane2}.bin");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpPostRgbStage40F0(Options options)
    {
        if (string.IsNullOrEmpty(options.Plane0Path) ||
            string.IsNullOrEmpty(options.Plane1Path) ||
            string.IsNullOrEmpty(options.Plane2Path))
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x40f0 dumping requires --plane0, --plane1, and --plane2.");
        }
        if (options.StageWidth <= 0 || options.StageHeight <= 0)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x40f0 dumping requires positive --stage-width and --stage-height.");
        }

        int planeBytes = options.StageWidth * options.StageHeight;
        byte[] plane0Input = File.ReadAllBytes(options.Plane0Path);
        byte[] plane1Input = File.ReadAllBytes(options.Plane1Path);
        byte[] plane2Input = File.ReadAllBytes(options.Plane2Path);
        if (plane0Input.Length < planeBytes ||
            plane1Input.Length < planeBytes ||
            plane2Input.Length < planeBytes)
        {
            throw new InvalidOperationException(
                "Post-RGB stage 0x40f0 inputs must each contain at least width*height bytes.");
        }
        int plane0Bytes = plane0Input.Length;
        int plane1Bytes = plane1Input.Length;
        int plane2Bytes = plane2Input.Length;

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr plane0 = Marshal.AllocHGlobal(plane0Bytes);
        IntPtr plane1 = Marshal.AllocHGlobal(plane1Bytes);
        IntPtr plane2 = Marshal.AllocHGlobal(plane2Bytes);

        try
        {
            Marshal.Copy(plane0Input, 0, plane0, plane0Bytes);
            Marshal.Copy(plane1Input, 0, plane1, plane1Bytes);
            Marshal.Copy(plane2Input, 0, plane2, plane2Bytes);

            IntPtr proc = IntPtr.Add(library, 0x40F0);
            InternalPostRgbStage40F0Delegate stage =
                (InternalPostRgbStage40F0Delegate)Marshal.GetDelegateForFunctionPointer(
                    proc,
                    typeof(InternalPostRgbStage40F0Delegate));

            stage(
                options.StageWidth,
                options.StageHeight,
                options.Level,
                options.Selector,
                plane0,
                plane1,
                plane2);

            byte[] outPlane0 = new byte[plane0Bytes];
            byte[] outPlane1 = new byte[plane1Bytes];
            byte[] outPlane2 = new byte[plane2Bytes];
            Marshal.Copy(plane0, outPlane0, 0, plane0Bytes);
            Marshal.Copy(plane1, outPlane1, 0, plane1Bytes);
            Marshal.Copy(plane2, outPlane2, 0, plane2Bytes);

            string outputPrefix = options.OutputPath;
            string directory = Path.GetDirectoryName(outputPrefix);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(outputPrefix + ".plane0.bin", outPlane0);
            File.WriteAllBytes(outputPrefix + ".plane1.bin", outPlane1);
            File.WriteAllBytes(outputPrefix + ".plane2.bin", outPlane2);
            Console.WriteLine(
                "dumped post-RGB stage 0x40f0 width=" + options.StageWidth +
                " height=" + options.StageHeight +
                " level=" + options.Level +
                " selector=" + options.Selector +
                " -> " + outputPrefix + ".{plane0,plane1,plane2}.bin");
        }
        finally
        {
            Marshal.FreeHGlobal(plane2);
            Marshal.FreeHGlobal(plane1);
            Marshal.FreeHGlobal(plane0);
            FreeLibrary(library);
        }

        return 0;
    }

    private static int DumpRowResample(Options options)
    {
        if (string.IsNullOrEmpty(options.RowInputPath))
        {
            throw new InvalidOperationException("Row-resample dumping requires --row-input.");
        }

        byte[] sourceBytes = File.ReadAllBytes(options.RowInputPath);
        int sourceBufferSize = Math.Max(sourceBytes.Length + 0x40, options.RowWidthLimit + 0x40);
        int outputBufferSize = Math.Max(options.RowOutputLength + 0x40, 0x400);

        IntPtr library = LoadLibrary(options.DllPath);
        if (library == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "LoadLibrary failed for " + options.DllPath + " (error " +
                Marshal.GetLastWin32Error() + ").");
        }

        IntPtr source = Marshal.AllocHGlobal(sourceBufferSize);
        IntPtr output = Marshal.AllocHGlobal(outputBufferSize);
        IntPtr lut = Marshal.AllocHGlobal(0x320);

        try
        {
            for (int index = 0; index < sourceBufferSize; index++)
            {
                Marshal.WriteByte(source, index, 0);
            }
            for (int index = 0; index < outputBufferSize; index++)
            {
                Marshal.WriteByte(output, index, 0);
            }
            for (int index = 0; index < 0x320; index++)
            {
                Marshal.WriteByte(lut, index, 0);
            }

            Marshal.Copy(sourceBytes, 0, source, sourceBytes.Length);
            Marshal.WriteInt32(IntPtr.Add(library, 0x1A0E0), options.RowModeE0);
            Marshal.WriteInt32(IntPtr.Add(library, 0x1A0E4), options.RowHelperModeE4);

            IntPtr lutProc = IntPtr.Add(library, 0x1210);
            InternalResampleLutDelegate lutBuilder =
                (InternalResampleLutDelegate)Marshal.GetDelegateForFunctionPointer(
                    lutProc,
                    typeof(InternalResampleLutDelegate));
            lutBuilder(lut, options.RowLutWidth, 0);

            IntPtr rowProc = IntPtr.Add(library, 0x1000);
            InternalRowResampleDelegate rowResample =
                (InternalRowResampleDelegate)Marshal.GetDelegateForFunctionPointer(
                    rowProc,
                    typeof(InternalRowResampleDelegate));

            int result = rowResample(
                source,
                output,
                lut,
                options.RowResetCounter,
                options.RowModeFlag,
                options.RowWidthLimit,
                options.RowOutputLength);
            if (result != 0)
            {
                throw new InvalidOperationException(
                    "Internal row resample returned " + result + ".");
            }

            int produced = Marshal.ReadInt32(IntPtr.Add(library, 0x1A0E8));
            if (produced < 0 || produced > outputBufferSize)
            {
                throw new InvalidOperationException(
                    "Unexpected produced-byte count from row resample: " + produced + ".");
            }

            byte[] outBytes = new byte[produced];
            Marshal.Copy(output, outBytes, 0, produced);

            string directory = Path.GetDirectoryName(options.OutputPath);
            if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            File.WriteAllBytes(options.OutputPath, outBytes);
            Console.WriteLine(
                "dumped row resample bytes=" + produced +
                " lut_width=" + options.RowLutWidth +
                " width_limit=" + options.RowWidthLimit +
                " output_length=" + options.RowOutputLength +
                " mode_e0=" + options.RowModeE0 +
                " mode_e4=" + options.RowHelperModeE4 +
                " -> " + Path.GetFileName(options.OutputPath));
        }
        finally
        {
            Marshal.FreeHGlobal(lut);
            Marshal.FreeHGlobal(output);
            Marshal.FreeHGlobal(source);
            FreeLibrary(library);
        }

        return 0;
    }

    private static void ValidateTrailer(byte[] trailer, bool allowUnsigned)
    {
        bool signatureMatches = true;
        for (int i = 0; i < ExpectedSignature.Length; i++)
        {
            if (trailer[i] != ExpectedSignature[i])
            {
                signatureMatches = false;
                break;
            }
        }

        if (!signatureMatches && !allowUnsigned)
        {
            throw new InvalidOperationException(
                "Trailer signature did not match c4b2e322; pass --force-unsigned-trailer to bypass.");
        }
    }

    private static void ZeroMemory(IntPtr pointer, int length)
    {
        for (int index = 0; index < length; index++)
        {
            Marshal.WriteByte(pointer, index, 0);
        }
    }

    private static int[] BuildSettingsBlock(Options options, byte[] trailer)
    {
        int[] block = new int[12];
        for (int i = 0; i < options.Settings.Length; i++)
        {
            block[i] = options.Settings[i];
        }

        block[7] = options.ExportMode;
        block[8] = trailer[9];
        block[9] = trailer[8];
        block[10] = trailer[11] + (10 * trailer[10]);
        block[11] = 1;
        return block;
    }

    private static void WriteBitmap(IntPtr imageBuffer, byte[] dibHeader, string outputPath)
    {
        int headerSize = ReadInt32(dibHeader, 0);
        if (headerSize != 40)
        {
            throw new InvalidOperationException(
                "Expected a 40-byte BITMAPINFOHEADER, got " + headerSize + ".");
        }

        int width = ReadInt32(dibHeader, 4);
        int height = ReadInt32(dibHeader, 8);
        int planes = ReadInt16(dibHeader, 12);
        int bitCount = ReadInt16(dibHeader, 14);
        int compression = ReadInt32(dibHeader, 16);
        int dibSizeImage = ReadInt32(dibHeader, 20);
        if (planes != 1 || bitCount != 24 || compression != 0)
        {
            throw new InvalidOperationException(
                "Unexpected DIB header: planes=" + planes +
                " bitCount=" + bitCount + " compression=" + compression + ".");
        }

        int stride = ((width * (bitCount / 8)) + 3) & ~3;
        int imageSize = dibSizeImage != 0 ? dibSizeImage : stride * Math.Abs(height);
        byte[] bmp = new byte[14 + 40 + imageSize];
        bmp[0] = (byte)'B';
        bmp[1] = (byte)'M';
        WriteInt32(bmp, 2, bmp.Length);
        WriteInt32(bmp, 10, 14 + 40);
        Array.Copy(dibHeader, 0, bmp, 14, dibHeader.Length);

        byte[] pixels = new byte[imageSize];
        Marshal.Copy(IntPtr.Add(imageBuffer, 40), pixels, 0, imageSize);
        Array.Copy(pixels, 0, bmp, 54, imageSize);

        string directory = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
        {
            Directory.CreateDirectory(directory);
        }

        File.WriteAllBytes(outputPath, bmp);
    }

    private static int ReadInt16(byte[] buffer, int offset)
    {
        return buffer[offset] | (buffer[offset + 1] << 8);
    }

    private static int ReadInt32(byte[] buffer, int offset)
    {
        return buffer[offset]
            | (buffer[offset + 1] << 8)
            | (buffer[offset + 2] << 16)
            | (buffer[offset + 3] << 24);
    }

    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        buffer[offset] = (byte)(value & 0xFF);
        buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
        buffer[offset + 2] = (byte)((value >> 16) & 0xFF);
        buffer[offset + 3] = (byte)((value >> 24) & 0xFF);
    }

    private static Options ParseArgs(string[] args)
    {
        Options options = new Options();
        Dictionary<string, Action<string>> handlers = new Dictionary<string, Action<string>>();
        handlers["--dll"] = delegate(string value) { options.DllPath = value; };
        handlers["--input"] = delegate(string value) { options.InputPath = value; };
        handlers["--output"] = delegate(string value) { options.OutputPath = value; };
        handlers["--converter-mode"] = delegate(string value) { options.ConverterMode = ParseInt(value); };
        handlers["--export-mode"] = delegate(string value) { options.ExportMode = ParseInt(value); };
        handlers["--setting0"] = delegate(string value) { options.Settings[0] = ParseInt(value); };
        handlers["--setting1"] = delegate(string value) { options.Settings[1] = ParseInt(value); };
        handlers["--setting2"] = delegate(string value) { options.Settings[2] = ParseInt(value); };
        handlers["--setting3"] = delegate(string value) { options.Settings[3] = ParseInt(value); };
        handlers["--setting4"] = delegate(string value) { options.Settings[4] = ParseInt(value); };
        handlers["--setting5"] = delegate(string value) { options.Settings[5] = ParseInt(value); };
        handlers["--setting6"] = delegate(string value) { options.Settings[6] = ParseInt(value); };
        handlers["--dump-resample-lut"] = delegate(string value) { options.DumpResampleLutWidth = ParseInt(value); };
        handlers["--helper-mode-e4"] = delegate(string value) { options.HelperModeE4 = ParseInt(value); };
        handlers["--geometry-preview-flag"] = delegate(string value) { options.GeometryPreviewFlag = ParseInt(value); };
        handlers["--geometry-export-mode"] = delegate(string value) { options.GeometryExportMode = ParseInt(value); };
        handlers["--geometry-selector"] = delegate(string value) { options.GeometrySelector = ParseInt(value); };
        handlers["--plane0"] = delegate(string value) { options.Plane0Path = value; };
        handlers["--plane1"] = delegate(string value) { options.Plane1Path = value; };
        handlers["--plane2"] = delegate(string value) { options.Plane2Path = value; };
        handlers["--normalize-preview-flag"] = delegate(string value) { options.NormalizePreviewFlag = ParseInt(value); };
        handlers["--normalize-stride"] = delegate(string value) { options.NormalizeStride = ParseInt(value); };
        handlers["--normalize-rows"] = delegate(string value) { options.NormalizeRows = ParseInt(value); };
        handlers["--source-preview-flag"] = delegate(string value) { options.SourcePreviewFlag = ParseInt(value); };
        handlers["--source-output-stride"] = delegate(string value) { options.SourceOutputStride = ParseInt(value); };
        handlers["--source-output-rows"] = delegate(string value) { options.SourceOutputRows = ParseInt(value); };
        handlers["--source-input"] = delegate(string value) { options.SourceInputPath = value; };
        handlers["--stage-width"] = delegate(string value) { options.StageWidth = ParseInt(value); };
        handlers["--stage-height"] = delegate(string value) { options.StageHeight = ParseInt(value); };
        handlers["--stage-param0"] = delegate(string value) { options.StageParam0 = ParseInt(value); };
        handlers["--stage-param1"] = delegate(string value) { options.StageParam1 = ParseInt(value); };
        handlers["--stage-param2"] = delegate(string value) { options.StageParam2 = ParseInt(value); };
        handlers["--delta0"] = delegate(string value) { options.Delta0Path = value; };
        handlers["--delta2"] = delegate(string value) { options.Delta2Path = value; };
        handlers["--scalar"] = delegate(string value) { options.Scalar = ParseDouble(value); };
        handlers["--level"] = delegate(string value) { options.Level = ParseInt(value); };
        handlers["--selector"] = delegate(string value) { options.Selector = ParseInt(value); };
        handlers["--threshold"] = delegate(string value) { options.Threshold = ParseInt(value); };
        handlers["--row-input"] = delegate(string value) { options.RowInputPath = value; };
        handlers["--row-lut-width"] = delegate(string value) { options.RowLutWidth = ParseInt(value); };
        handlers["--row-reset-counter"] = delegate(string value) { options.RowResetCounter = ParseInt(value); };
        handlers["--row-mode-flag"] = delegate(string value) { options.RowModeFlag = ParseInt(value); };
        handlers["--row-width-limit"] = delegate(string value) { options.RowWidthLimit = ParseInt(value); };
        handlers["--row-output-length"] = delegate(string value) { options.RowOutputLength = ParseInt(value); };
        handlers["--row-mode-e0"] = delegate(string value) { options.RowModeE0 = ParseInt(value); };
        handlers["--row-helper-mode-e4"] = delegate(string value) { options.RowHelperModeE4 = ParseInt(value); };

        for (int i = 0; i < args.Length; i++)
        {
            string arg = args[i];
            if (arg == "--force-unsigned-trailer")
            {
                options.ForceUnsignedTrailer = true;
                continue;
            }
            if (arg == "--trace-large-callsite-params")
            {
                options.TraceLargeCallsiteParams = true;
                continue;
            }
            if (arg == "--dump-geometry-stage")
            {
                options.DumpGeometryStage = true;
                continue;
            }
            if (arg == "--dump-row-resample")
            {
                options.DumpRowResample = true;
                continue;
            }
            if (arg == "--dump-normalize-stage")
            {
                options.DumpNormalizeStage = true;
                continue;
            }
            if (arg == "--dump-source-stage")
            {
                options.DumpSourceStage = true;
                continue;
            }
            if (arg == "--dump-source-prepass-4570")
            {
                options.DumpSourcePrepass4570 = true;
                continue;
            }
            if (arg == "--dump-post-geometry-prepare")
            {
                options.DumpPostGeometryPrepare = true;
                continue;
            }
            if (arg == "--dump-post-geometry-filter")
            {
                options.DumpPostGeometryFilter = true;
                continue;
            }
            if (arg == "--dump-post-geometry-center-scale")
            {
                options.DumpPostGeometryCenterScale = true;
                continue;
            }
            if (arg == "--dump-post-geometry-rgb-convert")
            {
                options.DumpPostGeometryRgbConvert = true;
                continue;
            }
            if (arg == "--dump-post-geometry-edge-response")
            {
                options.DumpPostGeometryEdgeResponse = true;
                continue;
            }
            if (arg == "--dump-post-geometry-dual-scale")
            {
                options.DumpPostGeometryDualScale = true;
                continue;
            }
            if (arg == "--dump-post-geometry-stage-2a00")
            {
                options.DumpPostGeometryStage2A00 = true;
                continue;
            }
            if (arg == "--dump-post-geometry-stage-2dd0")
            {
                options.DumpPostGeometryStage2DD0 = true;
                continue;
            }
            if (arg == "--dump-post-geometry-stage-4810")
            {
                options.DumpPostGeometryStage4810 = true;
                continue;
            }
            if (arg == "--dump-post-geometry-stage-3600")
            {
                options.DumpPostGeometryStage3600 = true;
                continue;
            }
            if (arg == "--dump-post-geometry-stage-3890")
            {
                options.DumpPostGeometryStage3890 = true;
                continue;
            }
            if (arg == "--dump-post-geometry-stage-3060")
            {
                options.DumpPostGeometryStage3060 = true;
                continue;
            }
            if (arg == "--dump-post-rgb-stage-3b00")
            {
                options.DumpPostRgbStage3B00 = true;
                continue;
            }
            if (arg == "--dump-post-rgb-stage-3f40")
            {
                options.DumpPostRgbStage3F40 = true;
                continue;
            }
            if (arg == "--dump-post-rgb-stage-42a0")
            {
                options.DumpPostRgbStage42A0 = true;
                continue;
            }
            if (arg == "--dump-post-rgb-stage-40f0")
            {
                options.DumpPostRgbStage40F0 = true;
                continue;
            }

            Action<string> handler;
            if (!handlers.TryGetValue(arg, out handler))
            {
                throw Usage("Unknown argument: " + arg);
            }

            if (i + 1 >= args.Length)
            {
                throw Usage("Missing value for " + arg);
            }

            i++;
            handler(args[i]);
        }

        if (string.IsNullOrEmpty(options.DllPath))
        {
            throw Usage("Required arguments are missing.");
        }

        if (options.DumpResampleLutWidth.HasValue)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpGeometryStage)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpRowResample)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpNormalizeStage)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpSourceStage)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpSourcePrepass4570)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryPrepare)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryFilter)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryCenterScale)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryRgbConvert)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryEdgeResponse)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryDualScale)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryStage2A00)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryStage2DD0)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryStage4810)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryStage3600)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryStage3890)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostGeometryStage3060)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostRgbStage3B00)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostRgbStage3F40)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostRgbStage42A0)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.DumpPostRgbStage40F0)
        {
            if (string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (options.TraceLargeCallsiteParams)
        {
            if (string.IsNullOrEmpty(options.InputPath) ||
                string.IsNullOrEmpty(options.OutputPath))
            {
                throw Usage("Required arguments are missing.");
            }
            return options;
        }

        if (string.IsNullOrEmpty(options.InputPath) ||
            string.IsNullOrEmpty(options.OutputPath))
        {
            throw Usage("Required arguments are missing.");
        }

        return options;
    }

    private static int ParseInt(string value)
    {
        return int.Parse(value, System.Globalization.CultureInfo.InvariantCulture);
    }

    private static double ParseDouble(string value)
    {
        return double.Parse(value, System.Globalization.CultureInfo.InvariantCulture);
    }

    private static Exception Usage(string message)
    {
        return new InvalidOperationException(
            message + Environment.NewLine +
            "Usage: Dj1000DllHarness.exe --dll PATH --input PATH --output PATH " +
            "[--converter-mode 0|1] [--export-mode 0|2|5] " +
            "[--setting0 N ... --setting6 N] [--force-unsigned-trailer]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --input PATH --output PATH " +
            "--trace-large-callsite-params [--converter-mode 0|1] [--export-mode 5] " +
            "[--setting0 N ... --setting6 N] [--force-unsigned-trailer]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-resample-lut WIDTH " +
            "[--helper-mode-e4 N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-geometry-stage " +
            "--plane0 PATH --plane1 PATH --plane2 PATH " +
            "[--geometry-preview-flag 0|1] [--geometry-export-mode 0|2|5] [--geometry-selector N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-normalize-stage " +
            "--plane0 PATH --plane1 PATH --plane2 PATH [--normalize-preview-flag 0|1] " +
            "[--normalize-stride N] [--normalize-rows N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-source-stage " +
            "--source-input PATH [--source-preview-flag 0|1] " +
            "[--source-output-stride N] [--source-output-rows N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-source-prepass-4570 " +
            "--source-input PATH [--source-preview-flag 0|1]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-prepare " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-filter " +
            "--stage-width N --stage-height N --delta0 PATH --delta2 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-center-scale " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-rgb-convert " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-post-geometry-edge-response " +
            "--stage-width N --stage-height N --plane0 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-dual-scale " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --scalar FLOAT" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-post-geometry-stage-2a00 " +
            "--stage-width N --stage-height N --plane0 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-stage-2dd0 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-stage-4810 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-post-geometry-stage-3600 " +
            "--stage-width N --stage-height N --plane0 PATH" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-geometry-stage-3890 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH [--level N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-post-geometry-stage-3060 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH " +
            "[--stage-param0 N] [--stage-param1 N] [--scalar FLOAT] [--threshold N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-rgb-stage-3b00 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH " +
            "[--level N] [--scalar FLOAT]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-rgb-stage-3f40 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH " +
            "[--level N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-rgb-stage-42a0 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH " +
            "[--stage-param0 N] [--stage-param1 N] [--stage-param2 N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PREFIX --dump-post-rgb-stage-40f0 " +
            "--stage-width N --stage-height N --plane0 PATH --plane1 PATH --plane2 PATH " +
            "[--level N] [--selector N]" + Environment.NewLine +
            "   or: Dj1000DllHarness.exe --dll PATH --output PATH --dump-row-resample " +
            "--row-input PATH [--row-lut-width N] [--row-width-limit N] [--row-output-length N] " +
            "[--row-reset-counter N] [--row-mode-flag N] [--row-mode-e0 N] [--row-helper-mode-e4 N]");
    }
}
