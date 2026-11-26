#!/usr/bin/env python3
"""Check actual GPU readback: primitives, RGBA, blending and dirty raster upload.
This is an integration diagnostic, not a gameplay or performance certification.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "../../work/lcs/runtime"

def ppm(name):
    with (RUNTIME / name).open("rb") as f:
        assert f.readline() == b"P6\n", "not a PPM capture"
        assert f.readline() == b"1280 720\n", "native target is not 1280x720"
        assert f.readline() == b"255\n"
        data = f.read()
    assert len(data) == 1280 * 720 * 3
    return data

def pixel(data, x, y):
    at = (y * 1280 + x) * 3
    return tuple(data[at:at+3])

def main():
    first, second, model = map(ppm, ("render-probe.ppm", "render-probe-2.ppm", "model-probe.ppm"))
    checks = []
    def check(label, x, y, expected):
        got = pixel(second, x, y)
        assert max(abs(a-b) for a,b in zip(got, expected)) <= 2, f"{label}: {got}, expected {expected}"
        checks.append({"check": label, "pixel": [x,y], "rgb": got})
    check("clear color", 1250, 700, (32,48,64))
    for x, kind in ((24,"strip"),(120,"indexed fan"),(216,"indexed triangles")):
        for dx,dy,rgb in ((16,16,(255,0,0)),(48,16,(0,255,0)),(16,48,(0,0,255)),(48,48,(255,255,255))):
            check(kind, x+dx,640+dy,rgb)
    check("vertex alpha 128", 352,672,(144,152,160))
    check("raster write invalidates GPU cache", 448,672,(0,255,255))
    # Catch missing output and runaway projection, without prescribing vehicle appearance.
    foreground = sum(tuple(model[i:i+3]) != (32,48,64) for i in range(0,len(model),3))
    assert 1000 < foreground < 1280*720*.8, f"unexpected model coverage: {foreground}"
    assert len(set(zip(first[::3],first[1::3],first[2::3]))) > 100, "HUD output is empty"
    result = {"status":"PASS","checks":checks,"model_foreground_pixels":foreground,
              "scope":"GPU readback for HUD textures and static model only; no game, audio, animation or FPS certification"}
    (ROOT / "logs/render-validation.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,indent=2))

if __name__ == "__main__":
    try:
        main()
    except (AssertionError,OSError) as e:
        print(f"FAIL: {e}",file=sys.stderr)
        sys.exit(1)
