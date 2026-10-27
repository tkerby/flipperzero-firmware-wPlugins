#!/usr/bin/env python3
"""Write decks/hsk5.tsv and decks/hsk6.tsv from the complete-hsk-vocabulary JSON.

    python tools/hsk_from_json.py complete.min.json

The source is https://github.com/drkameleon/complete-hsk-vocabulary (MIT),
whose "o5" and "o6" levels are the HSK 2.0 level 5 and 6 word lists. Each
entry's first form supplies the traditional characters, the numbered pinyin
and the meanings; the English column is the shortest meaning that fits the
card, cleaned of parentheses. Words are shuffled once with a fixed seed so
similar words are not introduced back to back.
"""
import json
import random
import re
import sys

MAX_HANZI = 4
TEXT_LEN = 24
MAX_ENGLISH = 22
SYLLABLE = re.compile(r"^[a-z]+[1-4]?$")


def pinyin(numbered):
    out = []
    for syl in numbered.replace("-", " ").split():
        syl = syl.lower().replace("u:", "v").replace("ü", "v").replace("ê", "e")
        syl = re.sub(r"[^a-z1-5]", "", syl)
        if syl.endswith("5") or syl.endswith("0"):
            syl = syl[:-1]
        if not SYLLABLE.match(syl):
            return None
        out.append(syl)
    return " ".join(out)


BAD_MEANING = re.compile(
    r"^(variant of|old variant of|see |used in |dialectal equivalent|\(common place name\)|surname )"
)
PLACE = re.compile(r"[A-Z][a-z]+ (county|district|city|township|prefecture) ")

# The HSK reading for words the dictionary lists under several readings
READING = {
    "唉": "ai1",
    "岸": "an4",
    "薄": "bao2",
    "背": "bei4",
    "便": "bian4",
    "布": "bu4",
    "踩": "cai3",
    "插": "cha1",
    "朝": "chao2",
    "趁": "chen4",
    "乘": "cheng2",
    "冲": "chong1",
    "丑": "chou3",
    "臭": "chou4",
    "大方": "da4 fang5",
    "挡": "dang3",
    "岛": "dao3",
    "倒": "dao4",
    "地道": "di4 dao5",
    "丁": "ding1",
    "翻": "fan1",
    "反复": "fan3 fu4",
    "方": "fang1",
    "仿佛": "fang3 fu2",
    "非": "fei1",
    "盖": "gai4",
    "官": "guan1",
    "管子": "guan3 zi5",
    "柜台": "gui4 tai2",
    "哈": "ha1",
    "和平": "he2 ping2",
    "横": "heng2",
    "胡同": "hu2 tong4",
    "节": "jie2",
    "结实": "jie1 shi5",
    "尽量": "jin3 liang4",
    "救": "jiu4",
    "举": "ju3",
    "卷": "juan3",
    "克": "ke4",
    "夸": "kua1",
    "老板": "lao3 ban3",
    "雷": "lei2",
    "龙": "long2",
    "露": "lu4",
    "骂": "ma4",
    "毛": "mao2",
    "摸": "mo1",
    "嫩": "nen4",
    "念": "nian4",
    "匹": "pi3",
    "片": "pian4",
    "飘": "piao1",
    "平": "ping2",
    "凭": "ping2",
    "浅": "qian3",
    "枪": "qiang1",
    "抢": "qiang3",
    "切": "qie1",
    "青": "qing1",
    "圈": "quan1",
    "绕": "rao4",
    "晒": "shai4",
    "蛇": "she2",
    "升": "sheng1",
    "诗": "shi1",
    "时代": "shi2 dai4",
    "收获": "shou1 huo4",
    "锁": "suo3",
    "塔": "ta3",
    "提": "ti2",
    "土地": "tu3 di4",
    "吐": "tu4",
    "团": "tuan2",
    "歪": "wai1",
    "闻": "wen2",
    "吻": "wen3",
    "系": "xi4",
    "吓": "xia4",
    "项": "xiang4",
    "胸": "xiong1",
    "延长": "yan2 chang2",
    "阳台": "yang2 tai2",
    "痒": "yang3",
    "摇": "yao2",
    "咬": "yao3",
    "夜": "ye4",
    "乙": "yi3",
    "晕": "yun1",
    "涨": "zhang3",
    "正": "zheng4",
    "支": "zhi1",
    "直": "zhi2",
    "钟": "zhong1",
    "重": "zhong4",
    "重复": "chong2 fu4",
    "煮": "zhu3",
    "资源": "zi1 yuan2",
    "呵": "he1",
    "挨": "ai2",
    "安宁": "an1 ning2",
    "熬": "ao2",
    "扒": "ba1",
    "把手": "ba3 shou5",
    "奔驰": "ben1 chi2",
    "本事": "ben3 shi5",
    "甭": "beng2",
    "扁": "bian3",
    "别致": "bie2 zhi4",
    "播种": "bo1 zhong3",
    "裁缝": "cai2 feng5",
    "澄清": "cheng2 qing1",
    "秤": "cheng4",
    "冲击": "chong1 ji1",
    "出息": "chu1 xi5",
    "幢": "zhuang4",
    "锤": "chui2",
    "词汇": "ci2 hui4",
    "刺": "ci4",
    "答复": "da2 fu4",
    "大意": "da4 yi5",
    "淡水": "dan4 shui3",
    "党": "dang3",
    "得罪": "de2 zui5",
    "吊": "diao4",
    "动荡": "dong4 dang4",
    "兜": "dou1",
    "端": "duan1",
    "恶心": "e3 xin1",
    "发布": "fa1 bu4",
    "番": "fan1",
    "方言": "fang1 yan2",
    "分量": "fen4 liang5",
    "丰满": "feng1 man3",
    "复兴": "fu4 xing1",
    "富裕": "fu4 yu4",
    "高明": "gao1 ming2",
    "搁": "ge1",
    "跟前": "gen1 qian5",
    "工夫": "gong1 fu5",
    "公道": "gong1 dao5",
    "罐": "guan4",
    "合并": "he2 bing4",
    "哄": "hong3",
    "回避": "hui2 bi4",
    "汇报": "hui4 bao4",
    "家伙": "jia1 huo5",
    "将军": "jiang1 jun1",
    "尽快": "jin3 kuai4",
    "井": "jing3",
    "局限": "ju2 xian4",
    "扛": "kang2",
    "磕": "ke1",
    "坑": "keng1",
    "孔": "kong3",
    "口音": "kou3 yin5",
    "款式": "kuan3 shi4",
    "啦": "la5",
    "利害": "li4 hai5",
    "联想": "lian2 xiang3",
    "淋": "lin2",
    "溜": "liu1",
    "搂": "lou3",
    "嘛": "ma5",
    "枚": "mei2",
    "蒙": "meng2",
    "眯": "mi1",
    "弥漫": "mi2 man4",
    "面子": "mian4 zi5",
    "嗯": "en4",
    "捏": "nie1",
    "拧": "ning2",
    "哦": "o4",
    "劈": "pi1",
    "颇": "po1",
    "铺": "pu1",
    "凄凉": "qi1 liang2",
    "翘": "qiao4",
    "人家": "ren2 jia5",
    "腮": "sai1",
    "盛": "sheng4",
    "拾": "shi2",
    "束": "shu4",
    "竖": "shu4",
    "数": "shu4",
    "耍": "shua3",
    "台风": "tai2 feng1",
    "掏": "tao1",
    "哇": "wa1",
    "网络": "wang3 luo4",
    "威信": "wei1 xin4",
    "温和": "wen1 he2",
    "污蔑": "wu1 mie4",
    "溪": "xi1",
    "兴隆": "xing1 long2",
    "凶恶": "xiong1 e4",
    "绣": "xiu4",
    "一目了然": "yi1 mu4 liao3 ran2",
    "翼": "yi4",
    "愈": "yu4",
    "熨": "yun4",
    "咋": "za3",
    "攒": "zan3",
    "赞同": "zan4 tong2",
    "扎": "zha1",
    "扎实": "zha1 shi5",
    "照应": "zhao4 ying5",
    "折": "zhe2",
    "振兴": "zhen4 xing1",
    "正当": "zheng4 dang4",
    "众所周知": "zhong4 suo3 zhou1 zhi1",
    "粥": "zhou1",
    "注释": "zhu4 shi4",
    "拽": "zhuai4",
    "组": "zu3",
    "琢磨": "zuo2 mo5",
}

# Traditional form to prefer when several forms share the HSK reading
TRAD = {"钟": "鐘", "系": "系", "升": "升"}

# Glosses for words whose dictionary meanings do not fit the card
OVERRIDES = {
    "方言": "dialect",
    "摄氏度": "degrees Celsius",
    "嗯": "mm; uh-huh",
    "气功": "qigong",
    "元宵节": "Lantern Festival",
    "简体字": "simplified character",
    "相辅相成": "to complement",
    "雪上加霜": "to make things worse",
    "咋": "how; why (dialect)",
    "钟": "clock; bell",
    "系": "system; department",
    "啦": "(sentence particle)",
    "便": "then; convenient",
    "克": "gram",
    "露": "to reveal; dew",
    "澄清": "to clarify",
    "番": "kind; sort",
    "蒙": "to cover; to deceive",
    "纪录": "record",
    "大厦": "large building",
    "寒暄": "to exchange greetings",
    "啰唆": "long-winded",
    "请示": "to ask instructions",
    "贤惠": "virtuous and capable",
    "粥": "congee; porridge",
    "做主": "to decide",
}


def english(meanings):
    cands = []
    for m in meanings:
        if BAD_MEANING.match(m) or PLACE.search(m):
            continue
        m = re.sub(r"\([^)]*\)", "", m)
        m = re.sub(r"\[[^]]*\]", "", m)
        m = m.replace("CL:", "").strip(" ;,")
        m = re.sub(r"\s+", " ", m)
        if not m or not m.isascii():
            continue
        parts = [m] + [m.split(sep)[0].strip() for sep in (";", ",") if sep in m]
        cands.append(parts)
    # the first meaning whole, then its first clause, then a word-boundary cut of it,
    # then the same for the later meanings
    for parts in cands:
        for c in parts:
            if 0 < len(c) <= MAX_ENGLISH:
                return c
        cut = parts[0][: MAX_ENGLISH + 1].rsplit(" ", 1)[0].strip(" ,;")
        if len(cut) >= 12:
            return cut
    return None


def pick_form(entry):
    """The form with the HSK reading, else the richest form that is not a proper noun."""
    forms = entry["f"]
    want = READING.get(entry["s"])
    if want:
        matches = [f for f in forms if f["i"]["n"].lower() == want]
        assert matches, f'{entry["s"]}: no form reads {want}'
        matches.sort(
            key=lambda f: (
                english(f["m"]) is None,
                f["t"] != TRAD.get(entry["s"], f["t"]),
                -len(f["m"]),
            )
        )
        return matches[0]
    usable = [f for f in forms if f["i"]["n"][:1].islower() and english(f["m"])]
    if usable:
        return max(usable, key=lambda f: len(f["m"]))
    for f in forms:
        if english(f["m"]):
            return f
    return forms[0]


def main():
    assert all(len(v) <= MAX_ENGLISH for v in OVERRIDES.values()), "override too long"
    data = json.load(open(sys.argv[1], encoding="utf-8"))
    used = set()
    for level in (5, 6):
        words = []
        skipped = []
        for e in data:
            if f"o{level}" not in e["l"]:
                continue
            s = e["s"]
            f = pick_form(e)
            t = f["t"]
            py = pinyin(f["i"]["n"])
            en = OVERRIDES.get(s) or english(f["m"])
            if (
                s in used
                or len(s) > MAX_HANZI
                or len(t) != len(s)
                or not py
                or len(py) >= TEXT_LEN
                or not en
            ):
                skipped.append((s, t, f["i"]["n"], f["m"][:2]))
                continue
            used.add(s)
            words.append((s, "=" if t == s else t, py, en))
        random.Random(level).shuffle(words)
        path = f"decks/hsk{level}.tsv"
        with open(path, "w", encoding="utf-8") as out:
            out.write(
                f"# HSK {level}. Columns: simplified, traditional (= if the same), pinyin, english.\n"
            )
            out.write(
                "# Pinyin: one syllable per space, tone digit 1-4 at the end, none for neutral tone, v for u-umlaut.\n"
            )
            out.write(
                "# From the HSK 2.0 word list in complete-hsk-vocabulary (MIT), shuffled once with a fixed seed.\n"
            )
            for w in words:
                out.write("\t".join(w) + "\n")
        print(f"{path}: {len(words)} words, {len(skipped)} skipped")
        for s in skipped:
            print("  skipped", s)


if __name__ == "__main__":
    main()
