"""Opt-in synthetic D3D11/CUDA color probe. Does not build or launch Magpie.

Use a matched 1.3.0.0 runtime directory and an external JSON report path.
Only synthetic bars are read back; no desktop/window content is captured.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--bridge', action='store_true', help='Use RGBA SDK I/O with same-format D3D copies and linear conversions')
parser.add_argument('--matrix', action='store_true', help='Test all 18 legal AI quality levels')
parser.add_argument('--explicit-encoding', action='store_true')
parser.add_argument('--width', type=int, default=960)
parser.add_argument('--height', type=int, default=540)
parser.add_argument('--packed-check', action='store_true', help='Also validate the shared RGB10A2/P32 export contract')
parser.add_argument('--packed-bridge', action='store_true', help='Use a lossless RGBA8 byte carrier for packed HDR10')
args = parser.parse_args()
if not (64 <= args.width <= 8192 and 64 <= args.height <= 8192):
    parser.error('Input dimensions must be in 64..8192 (2x stays within the D3D11 texture limit)')
sys.stdout.reconfigure(encoding='utf-8')
runtime = args.runtime.resolve(strict=True)
os.environ['NV_VIDEO_EFFECTS_PATH'] = str(runtime)
cookie = os.add_dll_directory(str(runtime))

def fn(lib, name, argtypes, restype=C.c_int):
    method = getattr(lib, name)
    method.argtypes, method.restype = argtypes, restype
    return method

def check(status, operation):
    if status:
        raise RuntimeError(f'{operation}: {status}')

def com(obj, index, argtypes, restype=C.c_long):
    table = C.cast(obj, C.POINTER(C.POINTER(C.c_void_p))).contents
    return C.WINFUNCTYPE(restype, C.c_void_p, *argtypes)(table[index])

def release(obj):
    if obj:
        com(obj, 2, [], C.c_ulong)(obj)

class Guid(C.Structure):
    _fields_ = [('a', W.DWORD), ('b', W.WORD), ('c', W.WORD), ('d', C.c_ubyte * 8)]

class Image(C.Structure):
    _fields_ = [('width', C.c_uint), ('height', C.c_uint), ('pitch', C.c_int),
                ('pixelFormat', C.c_int), ('componentType', C.c_int),
                ('pixelBytes', C.c_ubyte), ('componentBytes', C.c_ubyte),
                ('numComponents', C.c_ubyte), ('planar', C.c_ubyte),
                ('gpuMem', C.c_ubyte), ('colorspace', C.c_ubyte),
                ('reserved', C.c_ubyte * 2), ('pixels', C.c_void_p),
                ('deletePtr', C.c_void_p), ('deleteProc', C.c_void_p), ('bufferBytes', C.c_ulonglong)]

class TextureDesc(C.Structure):
    _fields_ = [(key, C.c_uint) for key in ('Width', 'Height', 'MipLevels', 'ArraySize',
                'Format', 'SampleCount', 'SampleQuality', 'Usage', 'BindFlags', 'CPUAccessFlags', 'MiscFlags')]

class Subresource(C.Structure):
    _fields_ = [('data', C.c_void_p), ('pitch', C.c_uint), ('slicePitch', C.c_uint)]

class Mapped(C.Structure):
    _fields_ = [('data', C.c_void_p), ('pitch', C.c_uint), ('depthPitch', C.c_uint)]

class AdapterDesc(C.Structure):
    _fields_ = [('Description', W.WCHAR * 128), ('VendorId', C.c_uint), ('DeviceId', C.c_uint),
                ('SubSysId', C.c_uint), ('Revision', C.c_uint), ('DedicatedVideoMemory', C.c_size_t),
                ('DedicatedSystemMemory', C.c_size_t), ('SharedSystemMemory', C.c_size_t),
                ('LuidLow', C.c_uint), ('LuidHigh', C.c_int)]

assert C.sizeof(Image) == 64 and Image.pixels.offset == 32
libs = {name: C.WinDLL(str(runtime / name), winmode=0x100 | 0x1000)
        for name in ['cudart64_12.dll', 'nvngxruntime.dll', 'NVCVImage.dll', 'NVVideoEffects.dll',
                     'nvngx_vsr.dll', 'nvVFXVideoSuperRes.dll']}
cv, vfx = libs['NVCVImage.dll'], libs['NVVideoEffects.dll']
cuda = C.WinDLL('nvcuda.dll', winmode=0x800)
ip = C.POINTER(Image)
alloc = fn(cv, 'NvCVImage_Alloc', [ip, C.c_uint, C.c_uint, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_uint])
dealloc = fn(cv, 'NvCVImage_Dealloc', [ip], None)
wrap = fn(cv, 'NvCVImage_InitFromD3D11Texture', [ip, C.c_void_p])
map_resource = fn(cv, 'NvCVImage_MapResource', [ip, C.c_void_p])
unmap_resource = fn(cv, 'NvCVImage_UnmapResource', [ip, C.c_void_p])
transfer = fn(cv, 'NvCVImage_Transfer', [ip, ip, C.c_float, C.c_void_p, ip])
init_image = fn(cv, 'NvCVImage_Init', [ip, C.c_uint, C.c_uint, C.c_int, C.c_void_p, C.c_int, C.c_int, C.c_uint, C.c_uint])
sync = fn(vfx, 'NvVFX_CudaStreamSynchronize', [C.c_void_p])
download = fn(cuda, 'cuMemcpyDtoH_v2', [C.c_void_p, C.c_uint64, C.c_size_t])
create = fn(vfx, 'NvVFX_CreateEffect', [C.c_char_p, C.POINTER(C.c_void_p)])
destroy = fn(vfx, 'NvVFX_DestroyEffect', [C.c_void_p], None)
set_image = fn(vfx, 'NvVFX_SetImage', [C.c_void_p, C.c_char_p, ip])
set_u32 = fn(vfx, 'NvVFX_SetU32', [C.c_void_p, C.c_char_p, C.c_uint])
set_stream = fn(vfx, 'NvVFX_SetCudaStream', [C.c_void_p, C.c_char_p, C.c_void_p])
load = fn(vfx, 'NvVFX_Load', [C.c_void_p])
run = fn(vfx, 'NvVFX_Run', [C.c_void_p, C.c_int])
stream_create = fn(vfx, 'NvVFX_CudaStreamCreate', [C.POINTER(C.c_void_p)])
stream_destroy = fn(vfx, 'NvVFX_CudaStreamDestroy', [C.c_void_p], None)
pop = fn(cuda, 'cuCtxPopCurrent_v2', [C.POINTER(C.c_void_p)])
ctx_release = fn(cuda, 'cuDevicePrimaryCtxRelease_v2', [C.c_int])
bars = [(224, 24, 16, 255), (24, 224, 16, 255), (16, 24, 224, 255),
        (0, 0, 0, 255), (255, 255, 255, 255), (177, 73, 29, 255), (80, 80, 80, 255)]
state = {'scope': 'Synthetic D3D11/NvCV/VSR on one GPU; not Magpie, capture, HDR or UI acceptance',
         'runtime': str(runtime), 'bridge': args.bridge, 'explicitEncoding': args.explicit_encoding,
         'dllSha256': {name: hashlib.sha256((runtime / name).read_bytes()).hexdigest() for name in libs}, 'cases': []}

def save():
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')

def descriptor(im):
    return {key: getattr(im, key) for key in ('width', 'height', 'pitch', 'pixelFormat', 'componentType',
                                             'pixelBytes', 'componentBytes', 'numComponents', 'planar', 'gpuMem')}

device, dc, adapter, dxgi_device, ctx, stream = [C.c_void_p() for _ in range(6)]
cu_device, level = C.c_int(), C.c_uint()
pushed = False

def texture(w, h, fmt, staging=False, initial=None):
    desc = TextureDesc(w, h, 1, 1, fmt, 1, 0, 3 if staging else 0, 0 if staging else 8, 0x20000 if staging else 0, 0)
    obj = C.c_void_p()
    data = Subresource(C.cast(initial, C.c_void_p), w * 4, 0) if initial is not None else None
    check(com(device, 5, [C.POINTER(TextureDesc), C.POINTER(Subresource), C.POINTER(C.c_void_p)])
          (device, C.byref(desc), C.byref(data) if data else None, C.byref(obj)), 'CreateTexture2D')
    return obj

def samples(data, pitch, w, h, fmt):
    values = []
    for index in range(len(bars)):
        x, y = int((index + .5) * w / len(bars)), h // 2
        points = [data[(y + dy) * pitch + (x + dx) * 4:(y + dy) * pitch + (x + dx) * 4 + 4]
                  for dy in range(-4, 5) for dx in range(-4, 5)]
        rgba = [round(sum(p[c] for p in points) / len(points), 2) for c in range(4)]
        if fmt == 7:
            rgba[0], rgba[2] = rgba[2], rgba[0]
        values.append(rgba)
    return values

def observe(record, label, values, exact=False):
    identity = all(abs(values[i][c] - bars[i][c]) <= .01 for i in range(len(bars)) for c in range(4)) if exact else (
        all(values[i][i] > max(values[i][c] for c in range(3) if c != i) + 32 for i in range(3)) and
        values[5][0] > values[5][1] > values[5][2])
    record['stages'].append({'stage': label, 'rgbaSamples': values, 'channelIdentity': identity})

def linear_samples(im):
    check(sync(stream), 'Sync readback')
    data = (C.c_ubyte * (im.pitch * im.height))()
    check(download(data, im.pixels, C.sizeof(data)), 'Read CUDA linear')
    return samples(bytes(data), im.pitch, im.width, im.height, im.pixelFormat)

def case(in_fmt, out_fmt, quality):
    record = {'inputDxgi': in_fmt, 'outputDxgi': out_fmt, 'quality': quality, 'stages': [], 'status': 'running'}
    state['cases'].append(record); save()
    images, textures, effect = [], [], C.c_void_p()
    def image(w, h, fmt):
        im = Image()
        check(alloc(C.byref(im), w, h, fmt, 1, 0, 1, 32), 'Alloc')
        images.append(im)
        return im
    def wrapped(tex):
        im = Image(); images.append(im)
        check(wrap(C.byref(im), tex), 'Wrap D3D texture')
        record.setdefault('initialDescriptors', []).append(descriptor(im))
        return im
    def move(src, dst):
        check(transfer(C.byref(src), C.byref(dst), 1, stream, None), 'Transfer')
    def mapped_move(wrapper, linear, importing):
        check(map_resource(C.byref(wrapper), stream), 'MapResource')
        try:
            record.setdefault('mappedDescriptors', []).append(descriptor(wrapper))
            move(wrapper, linear) if importing else move(linear, wrapper)
        finally:
            check(unmap_resource(C.byref(wrapper), stream), 'UnmapResource')
        check(sync(stream), 'Sync transfer')
    try:
        w, h = args.width, args.height
        ow, oh = (w, h) if quality is None or 8 <= quality <= 15 else (w * 2, h * 2)
        in_encoding, out_encoding = (7 if in_fmt == 87 else 6), (7 if out_fmt == 87 else 6)
        pixels = bytearray(w * h * 4)
        for y in range(h):
            for x in range(w):
                rgba = bars[min(x * len(bars) // w, len(bars) - 1)]
                pixels[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes(rgba if in_encoding == 6 else (rgba[2], rgba[1], rgba[0], rgba[3]))
        host = (C.c_ubyte * len(pixels)).from_buffer(pixels)
        source = texture(w, h, in_fmt, initial=host); textures.append(source)
        dest = texture(ow, oh, out_fmt); textures.append(dest)
        sw, dw = wrapped(source), wrapped(dest)
        for wrapper, tw, th, encoding in [(sw, w, h, in_encoding), (dw, ow, oh, out_encoding)]:
            if (wrapper.width, wrapper.height, wrapper.pixelFormat, wrapper.componentType,
                wrapper.planar, wrapper.gpuMem, wrapper.pixelBytes) != (tw, th, encoding, 1, 0, 3, 4):
                raise RuntimeError('Initial D3D wrapper contract mismatch: ' + str(descriptor(wrapper)))
        a = image(w, h, 6 if args.bridge else in_encoding)
        b = image(ow, oh, 6 if args.bridge else out_encoding)
        import_image = image(w, h, in_encoding) if args.bridge and in_encoding != 6 else a
        export_image = image(ow, oh, out_encoding) if args.bridge and out_encoding != 6 else b
        mapped_move(sw, import_image, True)
        observe(record, 'CUDA import', linear_samples(import_image), True)
        if import_image is not a:
            move(import_image, a)
            observe(record, 'linear BGRA -> RGBA', linear_samples(a), True)
        if quality is None:
            move(a, b)
        else:
            check(create(b'VideoSuperRes', C.byref(effect)), 'CreateEffect')
            check(set_stream(effect, b'CudaStream', stream), 'SetCudaStream')
            if args.explicit_encoding:
                check(set_u32(effect, b'ImageEncodingMode', 0), 'Set ImageEncodingMode RGB8')
            check(set_image(effect, b'SrcImage0', C.byref(a)), 'Set input')
            check(set_image(effect, b'DstImage0', C.byref(b)), 'Set output')
            check(set_u32(effect, b'QualityLevel', quality), 'Set quality')
            check(load(effect), 'Load VSR')
            check(run(effect, 0), 'Run VSR')
        observe(record, 'CUDA model output' if quality is not None else 'CUDA roundtrip', linear_samples(b), quality is None)
        if export_image is not b:
            move(b, export_image)
            observe(record, 'linear RGBA -> BGRA', linear_samples(export_image), quality is None)
        mapped_move(dw, export_image, False)
        staging = texture(ow, oh, out_fmt, staging=True); textures.append(staging)
        com(dc, 47, [C.c_void_p, C.c_void_p], None)(dc, staging, dest)
        mapped = Mapped()
        check(com(dc, 14, [C.c_void_p, C.c_uint, C.c_uint, C.c_uint, C.POINTER(Mapped)])
              (dc, staging, 0, 1, 0, C.byref(mapped)), 'Map staging')
        try:
            values = samples(C.string_at(mapped.data, mapped.pitch * oh), mapped.pitch, ow, oh, out_encoding)
        finally:
            com(dc, 15, [C.c_void_p, C.c_uint], None)(dc, staging, 0)
        observe(record, 'D3D export', values, quality is None)
        record['firstColorFailure'] = next((v['stage'] for v in record['stages'] if not v['channelIdentity']), None)
        record['status'] = 'color-fail' if record['firstColorFailure'] else 'pass'
    except Exception as error:
        record.update(status='api-fail', error=str(error))
    finally:
        sync(stream)
        if effect: destroy(effect)
        for im in reversed(images): dealloc(C.byref(im))
        for tex in reversed(textures): release(tex)
        save()
    print(json.dumps({key: record.get(key) for key in ('inputDxgi', 'outputDxgi', 'quality', 'status', 'firstColorFailure', 'error')}), flush=True)

def packed_check():
    record = {'case': 'RGB10A2/P32 byte-carrier roundtrip' if args.packed_bridge else 'RGB10A2/P32 direct roundtrip', 'status': 'running'}
    state['cases'].append(record)
    images, textures = [], []
    try:
        w, h = 64, 64
        host = (C.c_uint * (w * h))(*(((x * 17 + y * 7) & 1023) | (((x * 7 + y * 31) & 1023) << 10) |
                  (((x * 41 + y * 53) & 1023) << 20) | (((x + y) & 3) << 30)
                  for y in range(h) for x in range(w)))
        fmt = 28 if args.packed_bridge else 24
        source = texture(w, h, fmt, initial=host); textures.append(source)
        dest = texture(w, h, fmt); textures.append(dest)
        linear = Image(); images.append(linear)
        check(alloc(C.byref(linear), w, h, 13, 11, 0, 1, 32), 'Alloc RGB10A2/P32')
        if (linear.componentType, linear.planar, linear.gpuMem, linear.pixelBytes) != (11, 0, 1, 4):
            raise RuntimeError('Packed linear contract mismatch')
        record['linearDescriptor'] = descriptor(linear)
        transfer_image = linear
        if args.packed_bridge:
            transfer_image = Image(); images.append(transfer_image)
            check(init_image(C.byref(transfer_image), w, h, linear.pitch, linear.pixels, 6, 1, 0, 1), 'Init non-owning packed byte view')
            record['byteViewDescriptor'] = descriptor(transfer_image)
        record['wrapperDescriptors'] = []
        for tex, importing in [(source, True), (dest, False)]:
            wrapper = Image(); images.append(wrapper)
            check(wrap(C.byref(wrapper), tex), 'Wrap RGB10A2')
            record['wrapperDescriptors'].append(descriptor(wrapper))
            if (wrapper.width, wrapper.height, wrapper.pixelFormat, wrapper.componentType,
                wrapper.planar, wrapper.gpuMem, wrapper.pixelBytes) != (w, h, 6 if args.packed_bridge else 13, 1 if args.packed_bridge else 11, 0, 3, 4):
                raise RuntimeError('Packed wrapper contract mismatch')
            check(map_resource(C.byref(wrapper), stream), 'Map RGB10A2')
            try:
                a, b = (wrapper, transfer_image) if importing else (transfer_image, wrapper)
                check(transfer(C.byref(a), C.byref(b), 1, stream, None), 'Transfer RGB10A2')
            finally:
                check(unmap_resource(C.byref(wrapper), stream), 'Unmap RGB10A2')
            check(sync(stream), 'Sync RGB10A2')
        staging = texture(w, h, fmt, staging=True); textures.append(staging)
        com(dc, 47, [C.c_void_p, C.c_void_p], None)(dc, staging, dest)
        mapped = Mapped()
        check(com(dc, 14, [C.c_void_p, C.c_uint, C.c_uint, C.c_uint, C.POINTER(Mapped)])
              (dc, staging, 0, 1, 0, C.byref(mapped)), 'Map packed staging')
        try:
            expected = C.string_at(host, C.sizeof(host))
            rows = b''.join(C.string_at(mapped.data + y * mapped.pitch, w * 4) for y in range(h))
            if rows != expected: raise RuntimeError('Packed pixel bits changed during roundtrip')
            # CPU float32 emulation of the new shader's UNORM byte decoding.
            # This does not execute/compile the HLSL or validate PQ/scRGB output.
            if args.packed_bridge:
                from struct import pack, unpack
                f32 = lambda value: unpack('<f', pack('<f', value))[0]
                decoded = bytes(round(f32(f32(b / 255) * 255)) for b in rows)
                if decoded != expected: raise RuntimeError('UNORM byte reconstruction changed packed bits')
                record.update(bitExactPixels=w * h, cpuFloat32ByteReconstruction=True, hlslExecuted=False)
        finally:
            com(dc, 15, [C.c_void_p, C.c_uint], None)(dc, staging, 0)
        record['status'] = 'pass'
    except Exception as error:
        record.update(status='api-fail', error=str(error))
    finally:
        sync(stream)
        for im in reversed(images): dealloc(C.byref(im))
        for tex in reversed(textures): release(tex)
        save()
    print(json.dumps(record), flush=True)

try:
    d3d = C.WinDLL('d3d11.dll', winmode=0x800)
    create_device = fn(d3d, 'D3D11CreateDevice', [C.c_void_p, C.c_uint, C.c_void_p, C.c_uint, C.POINTER(C.c_uint),
                       C.c_uint, C.c_uint, C.POINTER(C.c_void_p), C.POINTER(C.c_uint), C.POINTER(C.c_void_p)], C.c_long)
    levels = (C.c_uint * 1)(0xb000)
    check(create_device(None, 1, None, 0x20, levels, 1, 7, C.byref(device), C.byref(level), C.byref(dc)), 'D3D11CreateDevice')
    iid = Guid(0x54ec77fa, 0x1377, 0x44e6, (C.c_ubyte * 8)(0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c))
    check(com(device, 0, [C.POINTER(Guid), C.POINTER(C.c_void_p)])(device, C.byref(iid), C.byref(dxgi_device)), 'Query IDXGIDevice')
    check(com(dxgi_device, 7, [C.POINTER(C.c_void_p)])(dxgi_device, C.byref(adapter)), 'GetAdapter')
    desc = AdapterDesc()
    check(com(adapter, 8, [C.POINTER(AdapterDesc)])(adapter, C.byref(desc)), 'GetDesc')
    state['gpu'] = {'name': desc.Description, 'vendor': desc.VendorId, 'device': desc.DeviceId,
                    'luidLow': desc.LuidLow, 'luidHigh': desc.LuidHigh}
    if desc.VendorId != 0x10de: raise RuntimeError('Default D3D11 adapter is not NVIDIA; no GPU substitution')
    check(fn(cuda, 'cuInit', [C.c_uint])(0), 'cuInit')
    check(fn(cuda, 'cuD3D11GetDevice', [C.POINTER(C.c_int), C.c_void_p])(C.byref(cu_device), adapter), 'Select same CUDA device')
    check(fn(cuda, 'cuDevicePrimaryCtxRetain', [C.POINTER(C.c_void_p), C.c_int])(C.byref(ctx), cu_device), 'Retain primary')
    check(fn(cuda, 'cuCtxPushCurrent_v2', [C.c_void_p])(ctx), 'Push primary'); pushed = True
    check(stream_create(C.byref(stream)), 'Create stream')
    version = C.c_uint()
    check(fn(vfx, 'NvVFX_GetVersion', [C.POINTER(C.c_uint)])(C.byref(version)), 'GetVersion')
    state['version'] = hex(version.value)
    driver = C.c_int()
    check(fn(cuda, 'cuDriverGetVersion', [C.POINTER(C.c_int)])(C.byref(driver)), 'CUDA driver version')
    state['cudaDriverVersion'] = driver.value
    if version.value != 0x01030000: raise RuntimeError('Requires Core 1.3.0.0')
    qualities = [1, 2, 3, 4, 16, 17, 18, 19, 21, 23, 8, 9, 10, 11, 12, 13, 14, 15] if args.matrix else [2]
    for in_fmt in (28, 87):
        for out_fmt in (28, 87):
            for quality in [None] + qualities:
                case(in_fmt, out_fmt, quality)
    if args.packed_check:
        packed_check()
    state['passCount'] = sum(c['status'] == 'pass' for c in state['cases'])
    state['failureCount'] = len(state['cases']) - state['passCount']
except Exception as error:
    state['fatalError'] = str(error)
finally:
    if stream: sync(stream); stream_destroy(stream)
    if pushed: removed = C.c_void_p(); pop(C.byref(removed))
    if ctx: ctx_release(cu_device)
    for obj in (adapter, dxgi_device, dc, device): release(obj)
    save()
print(json.dumps({'passed': state.get('passCount'), 'failed': state.get('failureCount'),
                  'fatalError': state.get('fatalError'), 'report': str(args.report)}), flush=True)
sys.exit(1 if state.get('fatalError') or state.get('failureCount') else 0)
