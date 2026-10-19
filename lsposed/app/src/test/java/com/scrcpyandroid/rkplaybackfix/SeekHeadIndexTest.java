// SPDX-License-Identifier: MIT
package com.scrcpyandroid.rkplaybackfix;

import org.junit.Test;
import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.List;
import static org.junit.Assert.*;

public class SeekHeadIndexTest {
    private byte[] concat(byte[]... items) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (byte[] item : items) out.write(item, 0, item.length);
        return out.toByteArray();
    }
    private byte[] uint(long value, int n) {
        byte[] out = new byte[n];
        for (int i = n - 1; i >= 0; --i) { out[i] = (byte)value; value >>>= 8; }
        return out;
    }
    private byte[] element(int id, int idBytes, byte[] body) {
        assertTrue(body.length < 127);
        return concat(uint(id, idBytes), new byte[]{(byte)(0x80 | body.length)}, body);
    }
    private byte[] entry(int id, long position) {
        return element(0x4dbb, 2, concat(element(0x53ab, 2, uint(id, 4)), element(0x53ac, 2, uint(position, 8))));
    }
    private byte[] head(byte[]... entries) { return element(SeekHeadIndex.HEAD, 4, concat(entries)); }
    private void invalid(byte[] data, long start, long size, long length) throws Exception {
        try { SeekHeadIndex.parse(data, start, size, length); fail("malformed index accepted"); }
        catch (SeekHeadIndex.Invalid expected) { }
    }
    @Test public void realOffsetsAndBranchingHeads() throws Exception {
        List<SeekHeadIndex.Link> links = SeekHeadIndex.parse(head(entry(SeekHeadIndex.HEAD, 900), entry(SeekHeadIndex.CUES, 800)), 52, 1000, 1052);
        assertEquals(2, links.size()); assertEquals(952, links.get(0).position); assertEquals(852, links.get(1).position);
        assertEquals(SeekHeadIndex.HEAD, links.get(0).id); assertEquals(SeekHeadIndex.CUES, links.get(1).id);
    }
    @Test public void unknownStreamLengthStillUsesSegmentBounds() throws Exception {
        assertEquals(1, SeekHeadIndex.parse(head(entry(SeekHeadIndex.CUES, 999)), 52, 1000, -1).size());
        invalid(head(entry(SeekHeadIndex.CUES, 1000)), 52, 1000, -1);
        assertEquals(1, SeekHeadIndex.parse(head(entry(SeekHeadIndex.CUES, 999)), 52, -1, -1).size());
    }
    @Test public void truncatedReadsNeverBecomeAnIndex() throws Exception {
        byte[] data = head(entry(SeekHeadIndex.CUES, 500));
        for (int n = 0; n < data.length; ++n) invalid(Arrays.copyOf(data, n), 52, 1000, 1052);
        assertEquals(data.length, SeekHeadIndex.headBytes(Arrays.copyOf(data, 12)));
    }
    @Test public void overflowAndOutOfStreamRejected() throws Exception {
        invalid(head(entry(SeekHeadIndex.CUES, Long.MAX_VALUE)), 52, -1, -1);
        invalid(head(entry(SeekHeadIndex.CUES, -1)), 52, -1, -1);
        invalid(head(entry(SeekHeadIndex.CUES, 1000)), 52, -1, 1052);
        invalid(head(entry(SeekHeadIndex.CUES, 1)), -1, -1, -1);
    }
    @Test public void unknownSizeAndInvalidVintRejected() throws Exception {
        invalid(new byte[]{0x11,0x4d,(byte)0x9b,0x74,(byte)0xff}, 0, -1, -1);
        invalid(new byte[]{0x11,0x4d,(byte)0x9b,0x74,0}, 0, -1, -1);
        invalid(new byte[]{0,0,0,0,0}, 0, -1, -1);
    }
    @Test public void duplicateFieldsAndEscapingChildRejected() throws Exception {
        byte[] id = element(0x53ab, 2, uint(SeekHeadIndex.CUES, 4));
        invalid(head(element(0x4dbb, 2, concat(id, id, element(0x53ac, 2, uint(10, 1))))), 0, 100, 100);
        byte[] data = head(entry(SeekHeadIndex.CUES, 10)); data[7] = (byte)0xfe;
        invalid(data, 0, 100, 100);
    }
    @Test public void unrelatedElementsDoNotCreateSeekability() throws Exception {
        byte[] skip = element(0xec, 1, new byte[]{1,2,3});
        assertTrue(SeekHeadIndex.parse(head(skip, entry(0x1549a966, 50)), 0, 100, 100).isEmpty());
    }
    @Test public void repeatedParsingIsTransactional() throws Exception {
        byte[] data = head(entry(SeekHeadIndex.HEAD, 200), entry(SeekHeadIndex.CUES, 300));
        for (int i = 0; i < 5; ++i) {
            invalid(Arrays.copyOf(data, data.length - 1), 52, 1000, -1);
            assertEquals(352, SeekHeadIndex.parse(data, 52, 1000, -1).get(1).position);
        }
    }
}
