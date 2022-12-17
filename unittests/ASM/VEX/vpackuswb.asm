%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM1": ["0x00007F4100007F41", "0x00007F4100007F41", "0x0000000000000000", "0x0000000000000000"],
    "XMM2": ["0x00007F4100007F41", "0x00007F4100007F41", "0x0000000000000000", "0x0000000000000000"],
    "XMM1": ["0x00007F4100007F41", "0x00007F4100007F41", "0x0000000000000000", "0x0000000000000000"],
    "XMM2": ["0x00007F4100007F41", "0x00007F4100007F41", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0x00007F4100007F41", "0x00007F4100007F41", "0x7F4100007F410000", "0x7F4100007F410000"],
    "XMM4": ["0x00007F4100007F41", "0x00007F4100007F41", "0x7F4100007F410000", "0x7F4100007F410000"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif

lea rdx, [rel .data]

; 16bit signed -> 8bit unsigned (saturated)
; input > 0x7F(SCHAR_MAX, 127) = 0xFF(UCHAR_MAX, 255)
; input < 0x00(Negative) = 0x0

vmovapd ymm0, [rdx]

vpackuswb xmm1, xmm0, [rdx]
vpackuswb xmm2, xmm0, xmm0

vpackuswb ymm3, ymm0, [rdx]
vpackuswb ymm4, ymm0, ymm0

hlt

align 32
.data:
dq 0x0000FFFF007F0041
dq 0x0000FFFF007F0041
dq 0x007F00410000FFFF
dq 0x007F00410000FFFF