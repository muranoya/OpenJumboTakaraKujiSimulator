#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "TinyMT/tinymt/tinymt32.h"

// 当選番号リスト
#define ID_FIRST_PRIZE        92169296
#define ID_FIRST_BEFORE_PRIZE (ID_FIRST_PRIZE-1)
#define ID_FIRST_AFTER_PRIZE  (ID_FIRST_PRIZE+1)
#define ID_SECOND_PRIZE1      20102239
#define ID_SECOND_PRIZE2      91186014
#define ID_THREE_PRIZE          190018
#define ID_FOUR_PRIZE              191
#define ID_FIVE_PRIZE               75
#define ID_SIX_PRIZE                 8

#define PRICE_OF_LOTTERY 300

// 賞のレベル
enum prize
{
    FIRST_PRIZE = 0,
    FIRST_BEFORE_AFTER_PRIZE,
    FIRST_GROUP_DIF_PRIZE,
    SECOND_PRIZE,
    THREE_PRIZE,
    FOUR_PRIZE,
    FIVE_PRIZE,
    SIX_PRIZE,
    NO_PRIZE,
};

// 賞金
static int prize[] = {
    500 * 1000 * 1000,
    100 * 1000 * 1000,
    100 * 1000,
    20 * 1000 * 1000,
    1000 * 1000,
    50 * 1000,
    3000,
    300,
    0,
};

// 既出番号か？
static char foregoing[1000000*100];
// 使用金額の合計
static uint64_t sum_money;
// 当選金額の合計
static uint64_t prize_money;
// SIGINTが送られたか
static int recv_sigint;
// 統計情報
static int stat[NO_PRIZE+1];

// 実行時オプション
static int      verbose;
static uint64_t budget;
static int      prize_level;
static int      seed;
static int      sleep_ms;
static double   recover_rate;

/* 関数プロトタイプ */
static void       sighandle(int h);
static uint32_t   buy_a_lottery(tinymt32_t *s);
static enum prize winning(uint32_t number);
static void       conv_lottery_str(enum prize p, char *out);
static void       conv_lottery_number(uint32_t num, char *out, int len);
static void       conv_money(uint64_t num, char *out, int len);
static void       show_stat();
static void       show_prize(enum prize p, uint32_t num);
static void       show_prize_level();
static void       show_help(const char *path);
static void       set_option(int argc, char *argv[]);

static void
sighandle(int h)
{
    switch (h)
    {
        case SIGINT:
            recv_sigint = 1;
    }
}

static uint32_t
buy_a_lottery(tinymt32_t *s)
{
    uint32_t r;
    while (1)
    {
        r = (uint32_t)(tinymt32_generate_uint32(s) * (99999999 + 1.0) / (1.0 + UINT32_MAX));
        if (foregoing[r])
        {
            continue;
        }
        else
        {
            sum_money += PRICE_OF_LOTTERY;
            foregoing[r] = 1;
            return r;
        }
    }
}

static enum prize
winning(uint32_t number)
{
    const uint32_t first6 = ID_FIRST_PRIZE - ID_FIRST_PRIZE/1000000 * 1000000;
    uint32_t lsb6, lsb4, lsb2, lsb1;
    enum prize ret = NO_PRIZE;

    lsb6 = number - (number / 1000000) * 1000000;
    lsb4 = number - (number / 10000) * 10000;
    lsb2 = number - (number / 100) * 100;
    lsb1 = number - (number / 10) * 10;

    if (number == ID_FIRST_PRIZE)
    {
        ret = FIRST_PRIZE;
    }
    else if (number == ID_FIRST_BEFORE_PRIZE || number == ID_FIRST_AFTER_PRIZE)
    {
        ret = FIRST_BEFORE_AFTER_PRIZE;
    }
    else if (lsb6 == first6)
    {
        ret = FIRST_GROUP_DIF_PRIZE;
    }
    else if (number == ID_SECOND_PRIZE1 || number == ID_SECOND_PRIZE2)
    {
        ret = SECOND_PRIZE;
    }
    else if (lsb6 == ID_THREE_PRIZE)
    {
        ret = THREE_PRIZE;
    }
    else if (lsb4 == ID_FOUR_PRIZE)
    {
        ret = FOUR_PRIZE;
    }
    else if (lsb2 == ID_FIVE_PRIZE)
    {
        ret = FIVE_PRIZE;
    }
    else if (lsb1 == ID_SIX_PRIZE)
    {
        ret = SIX_PRIZE;
    }

    if (prize_level >= 0 && ret <= prize_level) recv_sigint = 1;

    stat[ret]++;
    prize_money += prize[ret];

    return ret;
}

static void
conv_lottery_str(enum prize p, char *out)
{
    const char *s;

    switch (p)
    {
        case FIRST_PRIZE:              s = "　　　一等賞"; break;
        case FIRST_BEFORE_AFTER_PRIZE: s = "　一等前後賞"; break;
        case FIRST_GROUP_DIF_PRIZE:    s = "一等組違い賞"; break;
        case SECOND_PRIZE:             s = "　　　二等賞"; break;
        case THREE_PRIZE:              s = "　　　三等賞"; break;
        case FOUR_PRIZE:               s = "　　　四等賞"; break;
        case FIVE_PRIZE:               s = "　　　五等賞"; break;
        case SIX_PRIZE:                s = "　　　六等賞"; break;
        case NO_PRIZE:                 s = "　　　ハズレ"; break;
        default:                                           break;
    }
    strcpy(out, s);
}

static void
conv_lottery_number(uint32_t num, char *out, int len)
{
    int g = num / 1000000U;
    int i = num - g * 1000000U;
    snprintf(out, len, "%3d組%6d番", g == 0 ? 100 : g, i);
}

static void
conv_money(uint64_t m, char *out, int len)
{
    int w = 0;
    int chou = m / 10000U / 10000U / 10000U;
    int oku = m / 10000U / 10000U - chou * 10000U;
    int man = m / 10000U - chou * 10000U * 10000U - oku * 10000U;
    int en = m - m / 10000U * 10000U;

    if (chou != 0) w += snprintf(out+w, len-w, "%4d兆", chou);
    if (oku  != 0) w += snprintf(out+w, len-w, "%4d億", oku);
    if (man  != 0) w += snprintf(out+w, len-w, "%4d万", man);
    if (en   != 0) w += snprintf(out+w, len-w, "%4d", en);

    if (chou == 0 && oku == 0 && man == 0 && en == 0)
    {
        snprintf(out+w, len-w, "%4d円", 0);
    }
    else
    {
        snprintf(out+w, len-w, "円");
    }
}

static void
show_stat()
{
    int i;
    int64_t sum = 0;
    char temp1[64];
    char temp2[64];

    for (i = 0; i < NO_PRIZE+1; ++i)
    {
        sum += stat[i];
    }

    conv_money(sum_money, temp1, sizeof(temp1)/sizeof(char));
    conv_money(prize_money, temp2, sizeof(temp2)/sizeof(char));
    fprintf(stderr, "\n使用金額: %s\t当選金額: %s\t回収率: %3.3f%%\n",
            temp1, temp2, (double)prize_money / sum_money * 100.);

    for (i = 0; i < NO_PRIZE+1; ++i)
    {
        conv_lottery_str(i, temp1);
        fprintf(stderr, "%s: %8d\t%3.6f%%\n", temp1, stat[i], stat[i] / (double)sum * 100.);
    }
}

static void
show_prize(enum prize p, uint32_t num)
{
    char prize_str[64];
    char prize_money_str[64];
    char lottery_num_str[64];
    char sum_money_str[128];
    char prize_sum_money_str[128];

    if (p <= verbose)
    {
        conv_lottery_str(p, prize_str);
        conv_money(prize[p], prize_money_str, sizeof(prize_money_str)/sizeof(char));
        conv_lottery_number(num, lottery_num_str, sizeof(lottery_num_str)/sizeof(char));
        conv_money(sum_money, sum_money_str, sizeof(sum_money_str)/sizeof(char));
        conv_money(prize_money, prize_sum_money_str, sizeof(prize_money_str)/sizeof(char));
        fprintf(stderr,
                "%s\t%s\t%s\n"
                "使用金額: %s\t当選金額: %s\t回収率: %0.3f%%\n",
                lottery_num_str, prize_str, prize_money_str,
                sum_money_str, prize_sum_money_str, (double)prize_money / sum_money * 100.);
    }
}

static void
show_prize_level()
{
    char f1[64],             fm1[64],   fn1[64];
    char f1_1[64], f1_2[64], fm1_1[64], fn1_1[64];
    char                     fm1_2[64], fn1_2[64];
    char f2_1[64], f2_2[64], fm2[64],   fn2[64];
    char                     fm3[64],   fn3[64];
    char                     fm4[64],   fn4[64];
    char                     fm5[64],   fn5[64];
    char                     fm6[64],   fn6[64];

    conv_lottery_number(ID_FIRST_PRIZE, f1, sizeof(f1));
    conv_lottery_str(FIRST_PRIZE, fm1);
    conv_money(prize[FIRST_PRIZE], fn1, sizeof(fn1));

    conv_lottery_number(ID_FIRST_BEFORE_PRIZE, f1_1, sizeof(f1_1));
    conv_lottery_number(ID_FIRST_AFTER_PRIZE, f1_2, sizeof(f1_2));
    conv_lottery_str(FIRST_BEFORE_AFTER_PRIZE, fm1_1);
    conv_money(prize[FIRST_BEFORE_AFTER_PRIZE], fn1_1, sizeof(fn1_1));

    conv_lottery_str(FIRST_GROUP_DIF_PRIZE, fm1_2);
    conv_money(prize[FIRST_GROUP_DIF_PRIZE], fn1_2, sizeof(fn1_2));

    conv_lottery_number(ID_SECOND_PRIZE1, f2_1, sizeof(f2_1));
    conv_lottery_number(ID_SECOND_PRIZE2, f2_2, sizeof(f2_2));
    conv_lottery_str(SECOND_PRIZE, fm2);
    conv_money(prize[SECOND_PRIZE], fn2, sizeof(fn2));

    conv_lottery_str(THREE_PRIZE, fm3);
    conv_money(prize[THREE_PRIZE], fn3, sizeof(fn3));

    conv_lottery_str(FOUR_PRIZE, fm4);
    conv_money(prize[FOUR_PRIZE], fn4, sizeof(fn4));

    conv_lottery_str(FIVE_PRIZE, fm5);
    conv_money(prize[FIVE_PRIZE], fn5, sizeof(fn5));

    conv_lottery_str(SIX_PRIZE, fm6);
    conv_money(prize[SIX_PRIZE], fn6, sizeof(fn6));

    fprintf(stderr, "年末ジャンボ宝くじ2014年\n"
           "\t%s %s: %s\n"
           "\t%s %s: 一等の前後番号 %s %s\n"
           "\t%s %s: 一等の組違い番号\n"
           "\t%s %s: %s, %s\n"
           "\t%s %s: 各組共通%6d番\n"
           "\t%s %s: 下4桁%4d番\n"
           "\t%s %s: 下2桁%2d番\n"
           "\t%s %s: 下1桁%1d番\n",
           fm1,   fn1,   f1,
           fm1_1, fn1_1, f1_1, f1_2,
           fm1_2, fn1_2,
           fm2,   fn2,   f2_1, f2_2,
           fm3,   fn3,   ID_THREE_PRIZE,
           fm4,   fn4,   ID_FOUR_PRIZE,
           fm5,   fn5,   ID_FIVE_PRIZE,
           fm6,   fn6,   ID_SIX_PRIZE);
}

static void
show_help(const char *path)
{
    fprintf(stderr,
            "%s: [-v <賞のレベル>] [-u <金額>] [-p <賞のレベル>] [-s <乱数シード>] [-l <スリープ>]\n"
            "\t  [-r <パーセント>] [-h]\n\n"
            "\t-v: ログを出力する賞のレベル [デフォルト 7]\n\n"
            "\t-u: クジを購入するのに使用する最大の金額(円で指定)\n"
            "\t    0で無制限 [デフォルト]\n\n"
            "\t-p: 特定以上の賞のレベルが出るまで続ける\n"
            "\t    -1で無制限 [デフォルト]\n\n"
            "\t-s: 乱数シードの指定\n"
            "\t    無指定で時間 [デフォルト]\n\n"
            "\t-l: 一回クジを引く毎のスリープ時間(ミリ秒)\n"
            "\t    0ミリ秒 [デフォルト]\n\n"
            "\t-r: 回収率が指定したパーセント以上になると終了する(小数点数あり)\n"
            "\t    0.0以下で無制限 [デフォルト]\n\n"
            "\t-h: ヘルプを表示して終了する\n\n"
            "\t賞のレベル:\n"
            "\t    一等賞だけを出力する場合 0\n"
            "\t    一等前後賞以上を出力する場合 1\n"
            "\t    一等組違い賞以上を出力する場合 2\n"
            "\t    二等組賞以上を出力する場合 3\n"
            "\t    三等組賞以上を出力する場合 4\n"
            "\t    四等組賞以上を出力する場合 5\n"
            "\t    五等組賞以上を出力する場合 6\n"
            "\t    六等組賞以上を出力する場合 7\n\n"
            "\t金額:\n"
            "\t    0以上の正の整数\n\n"
            , path);
    show_prize_level();
}

static void
set_option(int argc, char *argv[])
{
    int result;

    verbose = SIX_PRIZE;
    budget = 0;
    prize_level = -1;
    seed = time(NULL);
    sleep_ms = 0;
    recover_rate = -1.0;

    while ((result = getopt(argc, argv, "v:u:p:s:l:r:h")) != -1)
    {
        switch (result)
        {
            case 'v':
                verbose = atoi(optarg);
                if (!(0 <= verbose && verbose <= 7))
                {
                    fprintf(stderr, "Error: Invalid prize level. %s\n", optarg);
                    show_help(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'u':
                budget = atoi(optarg);
                if (budget < 0)
                {
                    fprintf(stderr, "Error: Invalid money. %s\n", optarg);
                    show_help(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'p':
                prize_level = atoi(optarg);
                if (!(0 <= prize_level && prize_level <= 7))
                {
                    fprintf(stderr, "Error: Invalid prize level. %s\n", optarg);
                    show_help(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'l':
                sleep_ms = atoi(optarg);
                break;
            case 'r':
                recover_rate = atof(optarg);
                break;
            case 'h':
                show_help(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
        }
    }
}

int
main(int argc, char *argv[])
{
    struct timespec req;
    tinymt32_t r;
    uint32_t ret;
    int i, k;

    set_option(argc, argv);

    sum_money = 0;
    prize_money = 0;
    recv_sigint = 0;
    for (i = 0; i < 100*1000000; i++) foregoing[i] = 0;
    for (i = 0; i < NO_PRIZE+1; i++) stat[i] = 0;

    tinymt32_init(&r, seed);
    signal(SIGINT, sighandle);
    req.tv_sec = 0;
    req.tv_nsec = 1000 * 1000 * sleep_ms;

    show_prize_level();
    fprintf(stderr, "乱数シード: %u\n", seed);

    for (; !recv_sigint;)
    {
        ret = buy_a_lottery(&r);
        k = winning(ret);
        show_prize(k, ret);

        if (budget > 0 && sum_money + PRICE_OF_LOTTERY > budget) break;
        if (recover_rate > 0. && (double)prize_money / sum_money * 100. >= recover_rate) break;
        if (sleep_ms > 0) nanosleep(&req, NULL);
    }

    show_stat();

    return EXIT_SUCCESS;
}

