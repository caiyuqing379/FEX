%ifdef CONFIG
{
  "RegData": {
    "MM7":  ["0x8000000000000000", "0x4000"]
  },
  "Mode": "32BIT"
}
%endif

lea edx, [.data]

fld qword [edx + 8 * 0]
fimul dword [edx + 8 * 1]
hlt

.data:
dq 0x3ff0000000000000
dq 2
