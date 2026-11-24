/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Mike Tzou (Chocobo1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

import { expect, test, vi } from "vitest";

import "../../private/scripts/misc.js";

test("Test isHttpUrl()", () => {
    const isHttpUrl = window.qBittorrent.Misc.isHttpUrl;

    expect(isHttpUrl("http://example.com")).toBe(true);
    expect(isHttpUrl("https://example.com/path?a=b#c")).toBe(true);
    expect(isHttpUrl("HTTPS://EXAMPLE.COM")).toBe(true);
    expect(isHttpUrl("  https://example.com  ")).toBe(true);
    // relative URLs are resolved against the document URL
    expect(isHttpUrl("")).toBe(true);
    expect(isHttpUrl("/foo")).toBe(true);

    expect(isHttpUrl("javascript:alert(1)")).toBe(false);
    expect(isHttpUrl("JavaScript:alert(1)")).toBe(false);
    expect(isHttpUrl("  javascript:alert(1)")).toBe(false);
    expect(isHttpUrl("java\tscript:alert(1)")).toBe(false);
    expect(isHttpUrl("java\nscript:alert(1)")).toBe(false);
    expect(isHttpUrl("java\rscript:alert(1)")).toBe(false);
    expect(isHttpUrl("vbscript:msgbox(1)")).toBe(false);
    expect(isHttpUrl("data:text/html,<script>alert(1)</script>")).toBe(false);
    expect(isHttpUrl("blob:https://example.com/1234")).toBe(false);
    expect(isHttpUrl("file:///etc/passwd")).toBe(false);
    expect(isHttpUrl("ftp://example.com")).toBe(false);
    expect(isHttpUrl("mailto:someone@example.com")).toBe(false);
    expect(isHttpUrl("magnet:?xt=urn:btih:0000000000000000000000000000000000000000")).toBe(false);
});

test("Test filterInPlace()", () => {
    const filterInPlace = (array, predicate) => {
        window.qBittorrent.Misc.filterInPlace(array, predicate);
        return array;
    };

    expect(filterInPlace([], (() => true))).toStrictEqual([]);
    expect(filterInPlace([], (() => false))).toStrictEqual([]);
    expect(filterInPlace([1, 2, 3, 4], (() => true))).toStrictEqual([1, 2, 3, 4]);
    expect(filterInPlace([1, 2, 3, 4], (() => false))).toStrictEqual([]);
    expect(filterInPlace([1, 2, 3, 4], (x => (x % 2) === 0))).toStrictEqual([2, 4]);
});

test("Test parseVersion()", () => {
    const parseVersion = window.qBittorrent.Misc.parseVersion;

    expect(parseVersion("")).toStrictEqual({ valid: false });
    expect(parseVersion("1")).toStrictEqual({ valid: true, major: 1, minor: undefined, fix: undefined, patch: undefined });
    expect(parseVersion("a")).toStrictEqual({ valid: true, major: "a", minor: undefined, fix: undefined, patch: undefined });
    expect(parseVersion("ab")).toStrictEqual({ valid: true, major: "ab", minor: undefined, fix: undefined, patch: undefined });
    expect(parseVersion("NaN")).toStrictEqual({ valid: true, major: "NaN", minor: undefined, fix: undefined, patch: undefined });
    expect(parseVersion("1.2")).toStrictEqual({ valid: true, major: 1, minor: 2, fix: undefined, patch: undefined });
    expect(parseVersion("1.ab")).toStrictEqual({ valid: true, major: 1, minor: "ab", fix: undefined, patch: undefined });
    expect(parseVersion("1.2.3")).toStrictEqual({ valid: true, major: 1, minor: 2, fix: 3, patch: undefined });
    expect(parseVersion("1.2.3.4")).toStrictEqual({ valid: true, major: 1, minor: 2, fix: 3, patch: 4 });
    expect(parseVersion("a.b.c.d")).toStrictEqual({ valid: true, major: "a", minor: "b", fix: "c", patch: "d" });
});

test("Test compareVersions()", () => {
    const cmp = (left, right, result) => {
        const compareVersions = window.qBittorrent.Misc.compareVersions;
        const parseVersion = window.qBittorrent.Misc.parseVersion;

        if (result < 0) {
            expect(compareVersions(left, right)).toBeLessThan(0);
            expect(compareVersions(parseVersion(left), right)).toBeLessThan(0);
            expect(compareVersions(left, parseVersion(right))).toBeLessThan(0);
        }
        else if (result === 0) {
            expect(compareVersions(left, right)).toBe(0);
            expect(compareVersions(parseVersion(left), right)).toBe(0);
            expect(compareVersions(left, parseVersion(right))).toBe(0);
        }
        else {
            expect(compareVersions(left, right)).toBeGreaterThan(0);
            expect(compareVersions(parseVersion(left), right)).toBeGreaterThan(0);
            expect(compareVersions(left, parseVersion(right))).toBeGreaterThan(0);
        }
    };

    cmp("", "", 0);

    cmp("1", "", -1);
    cmp("", "1", 1);
    cmp("1", "1", 0);

    cmp("a", "", -1);
    cmp("", "a", 1);
    cmp("a", "a", 0);

    cmp("NaN", "1", 1);
    cmp("1", "NaN", -1);
    cmp("NaN", "NaN", 0);

    cmp("1", "2", -1);
    cmp("2", "1", 1);

    cmp("1", "1.1", -1);
    cmp("1.1", "1", 1);
    cmp("1.1", "1.1", 0);

    cmp("1.1", "1.a", -1);
    cmp("1.a", "1.1", 1);
    cmp("1.a", "1.a", 0);

    cmp("1.a", "1.ab", -1);
    cmp("1.ab", "1.a", 1);
    cmp("1.ab", "1.ab", 0);

    cmp("1.2.3.4", "99.4.5", -1);
    cmp("99.4.5", "1.2.3.4", 1);

    cmp("1.2.3.4", "1.2.3.5", -1);
    cmp("1.2.3.4", "1.2.3.4", 0);
});

test("Test toFixedPointString()", () => {
    const toFixedPointString = window.qBittorrent.Misc.toFixedPointString;

    expect(toFixedPointString(0, 0)).toBe("0");
    expect(toFixedPointString(0, 1)).toBe("0.0");
    expect(toFixedPointString(0, 2)).toBe("0.00");

    expect(toFixedPointString(0.1, 0)).toBe("0");
    expect(toFixedPointString(0.1, 1)).toBe("0.1");
    expect(toFixedPointString(0.1, 2)).toBe("0.10");

    expect(toFixedPointString(-0.1, 0)).toBe("0");
    expect(toFixedPointString(-0.1, 1)).toBe("-0.1");
    expect(toFixedPointString(-0.1, 2)).toBe("-0.10");

    expect(toFixedPointString(1.005, 0)).toBe("1");
    expect(toFixedPointString(1.005, 1)).toBe("1.0");
    expect(toFixedPointString(1.005, 2)).toBe("1.00");
    expect(toFixedPointString(1.005, 3)).toBe("1.005");
    expect(toFixedPointString(1.005, 4)).toBe("1.0050");

    expect(toFixedPointString(-1.005, 0)).toBe("-1");
    expect(toFixedPointString(-1.005, 1)).toBe("-1.0");
    expect(toFixedPointString(-1.005, 2)).toBe("-1.00");
    expect(toFixedPointString(-1.005, 3)).toBe("-1.005");
    expect(toFixedPointString(-1.005, 4)).toBe("-1.0050");

    expect(toFixedPointString(35.855, 0)).toBe("35");
    expect(toFixedPointString(35.855, 1)).toBe("35.8");
    expect(toFixedPointString(35.855, 2)).toBe("35.85");
    expect(toFixedPointString(35.855, 3)).toBe("35.855");
    expect(toFixedPointString(35.855, 4)).toBe("35.8550");

    expect(toFixedPointString(-35.855, 0)).toBe("-35");
    expect(toFixedPointString(-35.855, 1)).toBe("-35.8");
    expect(toFixedPointString(-35.855, 2)).toBe("-35.85");
    expect(toFixedPointString(-35.855, 3)).toBe("-35.855");
    expect(toFixedPointString(-35.855, 4)).toBe("-35.8550");

    expect(toFixedPointString(100, 0)).toBe("100");
    expect(toFixedPointString(100, 1)).toBe("100.0");
    expect(toFixedPointString(100, 2)).toBe("100.00");

    expect(toFixedPointString(-100, 0)).toBe("-100");
    expect(toFixedPointString(-100, 1)).toBe("-100.0");
    expect(toFixedPointString(-100, 2)).toBe("-100.00");
});

test("Test formatDate() - Format Coverage", () => {
    const formatDate = window.qBittorrent.Misc.formatDate;
    const testDate = new Date(2025, 7, 23, 22, 32, 46); // Aug 23, 2025 10:32:46 PM

    expect(formatDate(testDate, "MM/dd/yyyy, h:mm:ss AM/PM")).toBe("08/23/2025, 10:32:46 PM");
    expect(formatDate(testDate, "MM/dd/yyyy, HH:mm:ss")).toBe("08/23/2025, 22:32:46");
    expect(formatDate(testDate, "dd/MM/yyyy, HH:mm:ss")).toBe("23/08/2025, 22:32:46");
    expect(formatDate(testDate, "yyyy-MM-dd HH:mm:ss")).toBe("2025-08-23 22:32:46");
    expect(formatDate(testDate, "yyyy/MM/dd HH:mm:ss")).toBe("2025/08/23 22:32:46");
    expect(formatDate(testDate, "dd.MM.yyyy, HH:mm:ss")).toBe("23.08.2025, 22:32:46");
    expect(formatDate(testDate, "MMM dd, yyyy, h:mm:ss AM/PM")).toBe("Aug 23, 2025, 10:32:46 PM");
    expect(formatDate(testDate, "dd MMM yyyy, HH:mm:ss")).toBe("23 Aug 2025, 22:32:46");
});

test("Test formatDate() - Fallback Behavior", () => {
    // Mock ClientData.get
    const mockGet = vi.fn().mockReturnValue("default");
    const originalParent = window.parent;

    window.parent = {
        qBittorrent: {
            ClientData: {
                get: mockGet
            }
        }
    };

    const formatDate = window.qBittorrent.Misc.formatDate;
    const testDate = new Date(2025, 7, 23, 22, 32, 46); // Aug 23, 2025 10:32:46 PM
    const expectedDefault = testDate.toLocaleString();

    // Test that "default" format uses toLocaleString()
    expect(formatDate(testDate, "default")).toBe(expectedDefault);

    // Test default behavior when no format argument is provided
    expect(mockGet).toHaveBeenCalledTimes(0);
    expect(formatDate(testDate)).toBe(expectedDefault);
    expect(mockGet).toHaveBeenCalledWith("date_format");
    expect(mockGet).toHaveBeenCalledTimes(1);

    // Test with unknown/invalid format strings
    expect(formatDate(testDate, "invalid-format")).toBe(expectedDefault);
    expect(formatDate(testDate, "")).toBe(expectedDefault);
    expect(formatDate(testDate, null)).toBe(expectedDefault);

    expect(mockGet).toHaveBeenCalledTimes(1);
    expect(formatDate(testDate, undefined)).toBe(expectedDefault);
    expect(mockGet).toHaveBeenCalledWith("date_format");
    expect(mockGet).toHaveBeenCalledTimes(2);

    // Restore original window.parent
    window.parent = originalParent;
});

test("Test toItemFilter()", () => {
    const { toItemFilter, createItemFilter } = window.qBittorrent.Misc;
    const ALL = "all";

    expect(toItemFilter(ALL, ALL)).toStrictEqual(createItemFilter());
    expect(toItemFilter("movies", ALL)).toStrictEqual(createItemFilter(["movies"]));

    const filter = createItemFilter(["a"], ["b"]);
    expect(toItemFilter(filter, ALL)).toBe(filter);
});

test("Test updateItemFilter()", () => {
    const { updateItemFilter, createItemFilter } = window.qBittorrent.Misc;
    const ALL = "all";

    // plain click selects only the clicked item
    expect(updateItemFilter(createItemFilter(["a", "b"], ["c"]), "d", ALL)).toStrictEqual(createItemFilter(["d"]));
    expect(updateItemFilter(createItemFilter(["a", "b"], ["c"]), "a", ALL, { altKey: false })).toStrictEqual(createItemFilter(["a"]));

    // clicking "All" resets the filter, regardless of modifiers
    expect(updateItemFilter(createItemFilter(["a"], ["b"]), ALL, ALL, { ctrlKey: true })).toStrictEqual(createItemFilter());
    expect(updateItemFilter(createItemFilter(["a"], ["b"]), ALL, ALL, { altKey: true })).toStrictEqual(createItemFilter());

    // Ctrl/Cmd+click toggles inclusion
    expect(updateItemFilter(createItemFilter(), "a", ALL, { ctrlKey: true })).toStrictEqual(createItemFilter(["a"]));
    expect(updateItemFilter(createItemFilter(["a"]), "b", ALL, { metaKey: true })).toStrictEqual(createItemFilter(["a", "b"]));
    expect(updateItemFilter(createItemFilter(["a", "b"]), "a", ALL, { ctrlKey: true })).toStrictEqual(createItemFilter(["b"]));
    // ...and moves an excluded item to the included ones
    expect(updateItemFilter(createItemFilter([], ["a"]), "a", ALL, { ctrlKey: true })).toStrictEqual(createItemFilter(["a"]));

    // Alt/Option+click toggles exclusion
    expect(updateItemFilter(createItemFilter(), "a", ALL, { altKey: true })).toStrictEqual(createItemFilter([], ["a"]));
    expect(updateItemFilter(createItemFilter([], ["a"]), "a", ALL, { altKey: true })).toStrictEqual(createItemFilter());
    // ...and moves an included item to the excluded ones
    expect(updateItemFilter(createItemFilter(["a", "b"]), "a", ALL, { altKey: true })).toStrictEqual(createItemFilter(["b"], ["a"]));

    // Alt wins over Ctrl
    expect(updateItemFilter(createItemFilter(), "a", ALL, { altKey: true, ctrlKey: true })).toStrictEqual(createItemFilter([], ["a"]));

    // input filter is not modified
    const filter = createItemFilter(["a"]);
    updateItemFilter(filter, "b", ALL, { ctrlKey: true });
    expect(filter).toStrictEqual(createItemFilter(["a"]));
});

test("Test isItemFilterEqual() and isMultiItemFilter()", () => {
    const { isItemFilterEqual, isMultiItemFilter, createItemFilter } = window.qBittorrent.Misc;

    expect(isItemFilterEqual(createItemFilter(), createItemFilter())).toBe(true);
    expect(isItemFilterEqual(createItemFilter(["a", "b"], ["c"]), createItemFilter(["b", "a"], ["c"]))).toBe(true);
    expect(isItemFilterEqual(createItemFilter(["a"]), createItemFilter([], ["a"]))).toBe(false);
    expect(isItemFilterEqual(createItemFilter(["a"]), createItemFilter(["a", "b"]))).toBe(false);

    expect(isMultiItemFilter(createItemFilter())).toBe(false);
    expect(isMultiItemFilter(createItemFilter(["a"]))).toBe(false);
    expect(isMultiItemFilter(createItemFilter(["a", "b"]))).toBe(true);
    expect(isMultiItemFilter(createItemFilter([], ["a"]))).toBe(true);
});

test("Test serializeItemFilter() and parseItemFilter()", () => {
    const { serializeItemFilter, parseItemFilter, createItemFilter } = window.qBittorrent.Misc;

    const filter = createItemFilter(["a", "b/c"], ["d"]);
    expect(parseItemFilter(serializeItemFilter(filter))).toStrictEqual(filter);
    expect(parseItemFilter(serializeItemFilter(createItemFilter()))).toStrictEqual(createItemFilter());

    expect(parseItemFilter(null)).toBe(null);
    expect(parseItemFilter("")).toBe(null);
    expect(parseItemFilter("movies")).toBe(null);
    expect(parseItemFilter("{}")).toBe(null);
    expect(parseItemFilter("{\"included\":[1],\"excluded\":[]}")).toBe(null);
    expect(parseItemFilter("{\"included\":\"a\",\"excluded\":[]}")).toBe(null);
});

test("Test updateItemFilter() with single choice items", () => {
    const { updateItemFilter, createItemFilter } = window.qBittorrent.Misc;
    const ALL = "all";
    const SINGLE = ["error", "warning"];

    // single choice items are always chosen alone, regardless of modifiers
    expect(updateItemFilter(createItemFilter(["a"], ["b"]), "error", ALL, { ctrlKey: true }, SINGLE)).toStrictEqual(createItemFilter(["error"]));
    expect(updateItemFilter(createItemFilter(["a"], ["b"]), "warning", ALL, { altKey: true }, SINGLE)).toStrictEqual(createItemFilter(["warning"]));

    // including/excluding another item replaces the single choice item
    expect(updateItemFilter(createItemFilter(["error"]), "a", ALL, { ctrlKey: true }, SINGLE)).toStrictEqual(createItemFilter(["a"]));
    expect(updateItemFilter(createItemFilter(["error"]), "a", ALL, { altKey: true }, SINGLE)).toStrictEqual(createItemFilter([], ["a"]));

    // other items keep working as usual
    expect(updateItemFilter(createItemFilter(["a"]), "b", ALL, { metaKey: true }, SINGLE)).toStrictEqual(createItemFilter(["a", "b"]));
});
