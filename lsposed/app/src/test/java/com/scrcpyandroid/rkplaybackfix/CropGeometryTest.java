package com.scrcpyandroid.rkplaybackfix;

import java.util.ArrayDeque;
import org.junit.Test;
import static org.junit.Assert.*;

public class CropGeometryTest {
    private CropGeometry geometry(String codec, String renderer, int w, int h, int bw, int bh, int top) {
        CropGeometry g = new CropGeometry();
        assertTrue(g.observe("ffmpeg/video", codec + "_mediacodec: Output crop parameters top=" + top
            + " bottom=" + (top + h - 1) + " left=0 right=" + (w - 1)
            + ", resulting dimensions width=" + w + " height=" + h));
        assertTrue(g.observe("vo/" + renderer + "/aimagereader", "Texture dimensions changed to " + bw + "x" + bh));
        return g;
    }

    private boolean shaderRuns(CropGeometry g, int w, int h) {
        String expression = g.shader().split("//!WHEN ")[1].split("\n")[0];
        ArrayDeque<Double> stack = new ArrayDeque<>();
        for (String token : expression.split(" ")) {
            if (token.equals("!")) stack.push(stack.pop() == 0 ? 1.0 : 0.0);
            else if (token.equals("+") || token.equals("-") || token.equals("*")) {
                double b = stack.pop(), a = stack.pop();
                stack.push(token.equals("+") ? a + b : token.equals("-") ? a - b : a * b);
            } else stack.push(token.equals("HOOKED.w") ? (double) w : token.equals("HOOKED.h") ? (double) h : Double.parseDouble(token));
        }
        assertEquals(1, stack.size());
        return stack.pop() != 0;
    }

    @Test public void bothRenderersAcceptActiveAndPaddedTextures() {
        for (String renderer : new String[]{"gpu", "gpu-next"}) {
            CropGeometry g = geometry("hevc", renderer, 1920, 1080, 1920, 1104, 4);
            assertTrue(g.matches(1920, 1080, "mediacodec"));
            assertTrue(shaderRuns(g, 1920, 1080)); assertTrue(shaderRuns(g, 1920, 1104));
            assertFalse(shaderRuns(g, 960, 540)); assertFalse(shaderRuns(g, 1920, 1088));
            assertTrue(g.shader().contains("vec2(0.0, 4.0)"));
        }
    }

    @Test public void resolutionCodecAndOffsetComeFromMetadata() {
        CropGeometry g = geometry("vp9", "gpu-next", 1280, 720, 1280, 736, 8);
        assertTrue(g.matches(1280, 720, "mediacodec"));
        assertTrue(shaderRuns(g, 1280, 720)); assertTrue(shaderRuns(g, 1280, 736));
        assertTrue(g.shader().contains("vec2(0.0, 8.0)"));
        g = geometry("av1", "gpu", 3840, 2160, 3840, 2176, 2);
        assertTrue(g.matches(3840, 2160, "mediacodec"));
        assertTrue(shaderRuns(g, 3840, 2176)); assertFalse(shaderRuns(g, 1920, 1080));
    }

    @Test public void missingZeroInvalidOrMismatchedMetadataSkipsCorrection() {
        assertFalse(new CropGeometry().matches(1920, 1080, "mediacodec"));
        CropGeometry g = geometry("h264", "gpu", 1920, 1080, 1920, 1104, 0);
        assertFalse(g.matches(1920, 1080, "mediacodec"));
        g = geometry("h264", "gpu", 1920, 1080, 1920, 1080, 4);
        assertFalse(g.matches(1920, 1080, "mediacodec"));
        g = geometry("h264", "gpu", 1920, 1080, 1920, 1104, 4);
        assertFalse(g.matches(1280, 720, "mediacodec"));
        assertFalse(g.matches(1920, 1080, "mediacodec-copy"));
        assertFalse(g.matches(1920, 1080, "no"));
        assertFalse(g.matches(null, null, "mediacodec"));
        g.bottom--; assertFalse(g.matches(1920, 1080, "mediacodec"));
    }

    @Test public void unrelatedOrMalformedMessagesCannotActivateCorrection() {
        CropGeometry g = new CropGeometry();
        assertFalse(g.observe("other", "Texture dimensions changed to 1920x1104"));
        assertFalse(g.observe("vo/gpu-next", "Texture dimensions changed to 1920x1104"));
        assertFalse(g.observe("ffmpeg/video", "h264: Output crop parameters top=4 bottom=1083 left=0 right=1919, resulting dimensions width=1920 height=1080"));
        assertFalse(g.observe("vo/gpu/aimagereader", "Texture dimensions changed to 9999999999999999999x1104"));
        assertFalse(g.matches(1920, 1080, "mediacodec"));
    }
}
