// SPDX-License-Identifier: MIT
package com.scrcpyandroid.rkplaybackfix;

import java.util.ArrayList;
import java.util.List;

/** Bounded EBML SeekHead parsing only. Never invents a SeekMap or sample. */
final class SeekHeadIndex {
    static final int HEAD = 0x114d9b74, CUES = 0x1c53bb6b;
    static final int MAX_HEAD_BYTES = 65536;
    static final class Invalid extends Exception {
        Invalid(String message) { super(message); }
    }
    static final class Header {
        final long id, size;
        final int bytes;
        Header(long id, long size, int bytes) { this.id = id; this.size = size; this.bytes = bytes; }
    }
    static final class Link {
        final int id;
        final long position;
        Link(int id, long position) { this.id = id; this.position = position; }
    }
    private static int vintLength(int first, int max) throws Invalid {
        if (first == 0) throw new Invalid("zero EBML vint");
        int length = 1, bit = 0x80;
        while ((first & bit) == 0) { ++length; bit >>= 1; }
        if (length > max) throw new Invalid("oversize EBML vint");
        return length;
    }
    static Header header(byte[] data, int offset, int limit) throws Invalid {
        if (offset < 0 || limit > data.length || offset >= limit) throw new Invalid("truncated header");
        int idBytes = vintLength(data[offset] & 255, 4);
        if (idBytes >= limit - offset) throw new Invalid("truncated ID");
        long id = 0;
        for (int i = 0; i < idBytes; ++i) id = (id << 8) | (data[offset + i] & 255);
        int at = offset + idBytes, sizeBytes = vintLength(data[at] & 255, 8);
        if (sizeBytes > limit - at) throw new Invalid("truncated size");
        long size = data[at] & (0xff >> sizeBytes);
        for (int i = 1; i < sizeBytes; ++i) size = (size << 8) | (data[at + i] & 255);
        long unknown = (1L << (7 * sizeBytes)) - 1;
        if (size == unknown) throw new Invalid("unknown size in index");
        return new Header(id, size, idBytes + sizeBytes);
    }
    static int headBytes(byte[] prefix) throws Invalid {
        Header h = header(prefix, 0, prefix.length);
        if (h.id != HEAD || h.size > MAX_HEAD_BYTES - h.bytes) throw new Invalid("not a bounded SeekHead");
        return h.bytes + (int) h.size;
    }
    static boolean validPosition(long absolute, long segmentStart, long segmentSize, long inputLength) {
        if (segmentStart < 0 || absolute < segmentStart || absolute == Long.MAX_VALUE) return false;
        if (segmentSize >= 0 && absolute - segmentStart >= segmentSize) return false;
        return inputLength < 0 || absolute < inputLength;
    }
    private static long uint(byte[] data, int start, int end, int max) throws Invalid {
        if (start >= end || end - start > max || (end - start == 8 && data[start] < 0))
            throw new Invalid("invalid unsigned index integer");
        long value = 0;
        for (int i = start; i < end; ++i) value = (value << 8) | (data[i] & 255);
        return value;
    }
    static List<Link> parse(byte[] data, long segmentStart, long segmentSize, long inputLength) throws Invalid {
        int total = headBytes(data);
        if (data.length != total) throw new Invalid("incomplete SeekHead");
        Header root = header(data, 0, data.length);
        ArrayList<Link> result = new ArrayList<>();
        int elements = 0;
        for (int p = root.bytes; p < total;) {
            if (++elements > 4096) throw new Invalid("too many index elements");
            Header entry = header(data, p, total);
            int body = p + entry.bytes;
            if (entry.size > total - body) throw new Invalid("index child out of bounds");
            int end = body + (int) entry.size;
            if (entry.id == 0x4dbb) {
                long id = -1, relative = -1;
                for (int q = body; q < end;) {
                    if (++elements > 4096) throw new Invalid("too many index fields");
                    Header field = header(data, q, end);
                    int value = q + field.bytes;
                    if (field.size > end - value) throw new Invalid("index field out of bounds");
                    int next = value + (int) field.size;
                    if (field.id == 0x53ab) {
                        if (id != -1) throw new Invalid("duplicate SeekID");
                        id = uint(data, value, next, 4);
                    } else if (field.id == 0x53ac) {
                        if (relative != -1) throw new Invalid("duplicate SeekPosition");
                        relative = uint(data, value, next, 8);
                    }
                    q = next;
                }
                if (id == HEAD || id == CUES) {
                    if (relative < 0 || segmentStart < 0 || relative > Long.MAX_VALUE - segmentStart)
                        throw new Invalid("invalid index offset");
                    long absolute = segmentStart + relative;
                    if (!validPosition(absolute, segmentStart, segmentSize, inputLength))
                        throw new Invalid("index offset outside stream");
                    result.add(new Link((int) id, absolute));
                    if (result.size() > 256) throw new Invalid("too many index links");
                }
            }
            p = end;
        }
        return result;
    }
}
