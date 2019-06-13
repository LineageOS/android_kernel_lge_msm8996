/*
 *  ES9218P Device Driver.
 *
 *  This program is device driver for ES9218P chip set.
 *  The ES9218P is a high-performance 32-bit, 2-channel audio SABRE HiFi D/A converter
 *  with headphone amplifier, analog volume control and output switch designed for
 *  audiophile-grade portable application such as mobile phones and digital music player,
 *  consumer applications such as USB DACs and A/V receivers, as well as professional
 *  such as mixer consoles and digital audio workstations.
 *
 *  Copyright (C) 2016, ESS Technology International Ltd.
 *
 */


#ifndef __ES9218P_H__
#define __ES9218P_H__

#define ES9218P_REG_00                      0
#define ES9218P_REG_01                      1
#define ES9218P_REG_02                      2
#define ES9218P_REG_03                      3
#define ES9218P_REG_04                      4
#define ES9218P_REG_05                      5
#define ES9218P_REG_06                      6
#define ES9218P_REG_07                      7
#define ES9218P_REG_08                      8
#define ES9218P_REG_09                      9
#define ES9218P_REG_10                      10
#define ES9218P_REG_11                      11
#define ES9218P_REG_12                      12
#define ES9218P_REG_13                      13
#define ES9218P_REG_14                      14
#define ES9218P_REG_15                      15
#define ES9218P_REG_16                      16
#define ES9218P_REG_17                      17
#define ES9218P_REG_18                      18
#define ES9218P_REG_19                      19
#define ES9218P_REG_20                      20
#define ES9218P_REG_21                      21
#define ES9218P_REG_22                      22
#define ES9218P_REG_23                      23
#define ES9218P_REG_24                      24
#define ES9218P_REG_25                      25
#define ES9218P_REG_26                      26
#define ES9218P_REG_27                      27
#define ES9218P_REG_28                      28
#define ES9218P_REG_29                      29
#define ES9218P_REG_30                      30
#define ES9218P_REG_31                      31
#define ES9218P_REG_32                      32
#define ES9218P_REG_33                      33
#define ES9218P_REG_34                      34
#define ES9218P_REG_35                      35
#define ES9218P_REG_36                      36
#define ES9218P_REG_37                      37
#define ES9218P_REG_38                      38
#define ES9218P_REG_39                      39
#define ES9218P_REG_40                      40
#define ES9218P_REG_41                      41
#define ES9218P_REG_42                      42
#define ES9218P_REG_43                      43
#define ES9218P_REG_44                      44
#define ES9218P_REG_45                      45
#define ES9218P_REG_46                      46
#define ES9218P_REG_47                      47
#define ES9218P_REG_48                      48
#define ES9218P_REG_49                      49
#define ES9218P_REG_50                      50
#define ES9218P_REG_51                      51
#define ES9218P_REG_52                      52
#define ES9218P_REG_53                      53
#define ES9218P_REG_54                      54
#define ES9218P_REG_55                      55
#define ES9218P_REG_56                      56
#define ES9218P_REG_57                      57
#define ES9218P_REG_58                      58
#define ES9218P_REG_59                      59
#define ES9218P_REG_60                      60
#define ES9218P_REG_61                      61
#define ES9218P_REG_62                      62
#define ES9218P_REG_63                      63
#define ES9218P_REG_64                      64
#define ES9218P_REG_65                      65
#define ES9218P_REG_66                      66
#define ES9218P_REG_67                      67
#define ES9218P_REG_68                      68
#define ES9218P_REG_69                      69
#define ES9218P_REG_70                      70
#define ES9218P_REG_71                      71
#define ES9218P_REG_72                      72
#define ES9218P_REG_73                      73
#define ES9218P_REG_74                      74
#define ES9218P_REG_75                      75
#define ES9218P_REG_76                      76
#define ES9218P_REG_77                      77
#define ES9218P_REG_78                      78
#define ES9218P_REG_79                      79
#define ES9218P_REG_80                      80



#define ES9218P_SYSTEM_REG                  0
#define ES9218P_INPUT_SELECT                1
#define ES9218P_AUTOMUTE_CONFIG             2
#define ES9218P_ANALOG_VOL_CTRL             3
#define ES9218P_AUTOMUTE_TIME               4
#define ES9218P_AUTOMUTE_LEVEL              5
#define ES9218P_DOP_VOL_RAMP_RATE           6
#define ES9218P_FILTER_BAND_SYSTEM_MUTE     7
#define ES9218P_GPIO1_2_CONFIG              8
#define ES9218P_REG09_RESERVED              9
#define ES9218P_MASTERMODE_SYNC_CONFIG      10
#define ES9218P_OVERCURRENT_PROTECT         11
#define ES9218P_DPLL_BANDWIDTH              12
#define ES9218P_THD_COMP_MONO_MODE          13
#define ES9218P_SOFT_START_CONFIG           14
#define ES9218P_VOL1_CTRL                   15
#define ES9218P_VOL2_CTRL                   16
#define ES9218P_MASTERTRIM_4                17
#define ES9218P_MASTERTRIM_3                18
#define ES9218P_MASTERTRIM_2                19
#define ES9218P_MASTERTRIM_1                20
#define ES9218P_GPIO_INPUT_SEL              21
#define ES9218P_THD_COMP_C2_2               22
#define ES9218P_THD_COMP_C2_1               23
#define ES9218P_THD_COMP_C3_2               24
#define ES9218P_THD_COMP_C3_1               25
#define ES9218P_CP_SS_DELAY                 26
#define ES9218P_GEN_CONFIG                  27
#define ES9218P_REG28_RESERVED              28
#define ES9218P_AUTO_CLK_GEAR               29
#define ES9218P_CP_CLOCK_2                  30
#define ES9218P_CP_CLOCK_1                  31
#define ES9218P_AMP_CONFIG                  32
#define ES9218P_REG33_RESERVED              33
#define ES9218P_NCO_NUM_4                   34
#define ES9218P_NCO_NUM_3                   35
#define ES9218P_NCO_NUM_2                   36
#define ES9218P_NCO_NUM_1                   37
#define ES9218P_REG38_RESERVED              38
#define ES9218P_REG39_RESERVED              39
#define ES9218P_FIR_RAM_ADDR                40
#define ES9218P_FIR_RAM_DATA_3              41
#define ES9218P_FIR_RAM_DATA_2              42
#define ES9218P_FIR_RAM_DATA_1              43
#define ES9218P_FIR_CONFIG                  44
#define ES9218P_ANALOG_CTRL_OVERRIDE        45
#define ES9218P_REG46_RESERVED              46
#define ES9218P_REG47_RESERVED              47
#define ES9218P_REG48_RESERVED              48
#define ES9218P_CLK_GEAR_THRESH_3           49
#define ES9218P_CLK_GEAR_THRESH_2           50
#define ES9218P_CLK_GEAR_THRESH_1           51
#define ES9218P_REG52_RESERVED              52
#define ES9218P_REG53_RESERVED              53
#define ES9218P_REG54_RESERVED              54
#define ES9218P_REG55_RESERVED              55
#define ES9218P_REG56_RESERVED              56
#define ES9218P_REG57_RESERVED              57
#define ES9218P_REG58_RESERVED              58
#define ES9218P_REG59_RESERVED              59
#define ES9218P_REG60_RESERVED              60

#define ES9218P_CHIPSTATUS                  64
#define ES9218P_GPIO_READBACK               65
#define ES9218P_DPLL_NUM_4                  66
#define ES9218P_DPLL_NUM_3                  67
#define ES9218P_DPLL_NUM_2                  68
#define ES9218P_DPLL_NUM_1                  69
#define ES9218P_REG70_RESERVED              70
#define ES9218P_REG71_RESERVED              71
#define ES9218P_INPUT_SEL_AUTOMUTE_STATUS   72
#define ES9218P_RAM_COEFF_OUT_3             73
#define ES9218P_RAM_COEFF_OUT_2             74
#define ES9218P_RAM_COEFF_OUT_1             75



#define ES9218P_I2C_LEN_MASK                0xC0
#define ES9218P_SOFT_START_MASK             0x80


#define INPUT_CONFIG_SOURCE                 1
#define I2S_BIT_FORMAT_MASK                 (0x03 << 6)
#define MASTER_MODE_CONTROL                 10
#define I2S_CLK_DIVID_MASK                  (0x03 << 5)
#define RIGHT_CHANNEL_VOLUME_15             15
#define LEFT_CHANNEL_VOLUME_16              16
#define MASTER_TRIM_VOLUME_17               17
#define MASTER_TRIM_VOLUME_18               18
#define MASTER_TRIM_VOLUME_19               19
#define MASTER_TRIM_VOLUME_20               20

#define CLK_GEAR_MASK                       0x0C
#define AVC_EN_MASK                         0x20
#define AMP_OC_EN                           0x04
#define AREG_OUT_MASK                       0x30
#define AMP_MODE_MASK                       0x07
#define DOP_EN_MASK                         0x08
#define PRESET_FILTER_MASK                  0xE0



enum {
    ESS_PS_CLOSE,
    ESS_PS_OPEN,
    ESS_PS_BYPASS,
    ESS_PS_HIFI,
    ESS_PS_IDLE,
    ESS_PS_ACTIVE,
};


struct es9218_priv {
    struct  snd_soc_codec   *codec;
    struct  i2c_client      *i2c_client;
    struct  es9218_data     *es9218_data;
    struct  delayed_work    hifi_in_standby_work;
    struct  delayed_work    sleep_work;
    struct  mutex           power_lock;
    struct  wake_lock       sleep_lock;
    struct  wake_lock       shutdown_lock;
} es9218_priv;


struct es9218_data {
    int     reset_gpio;     //HIFI_RESET_N
    int     power_gpio;     //HIFI_LDO_SW
    int     hph_switch;     //HIFI_MODE2
    int     ear_dbg;
    int     hw_rev;
#ifdef DEDICATED_I2C
    int     i2c_scl_gpio;
    int     i2c_sda_gpio;
    int     i2c_addr;
#endif
#ifdef USE_CONTROL_EXTERNAL_LDO_FOR_DVDD
    struct regulator    *vreg_dvdd;
    u32                 low_vol_level;
    u32                 high_vol_level;
#endif
};


enum sabre_filter_shape {
    SABRE_FILTER_FASTROLLOFF,
    SABRE_FILTER_SLOWROLLOFF,
    SABRE_FILTER_MINIMUM,
    SABRE_FILTER_MINSLOW,
    SABRE_FILTER_APODFAST1,
    SABRE_FILTER_HYBRIDFAST = 6,
    SABRE_FILTER_BRICKWALL
};

enum sablre_filter_symmetry{
    SABRE_FILTER_SYMMETRY_SINE,
    SABRE_FILTER_SYMMETRY_COSINE
};

struct sabre_custom_filter {
    enum    sabre_filter_shape      shape;      ///< roll-off shape of filter
    enum    sablre_filter_symmetry  symmetry;   ///< symmetry type of stage 2 filter coefficients
    int     stage1_coeff[128];
    int     stage2_coeff[16];
};

struct sabre_custom_filter es9218_sabre_custom_ft[] = {
  {
    SABRE_FILTER_FASTROLLOFF,       // custom filter type
    SABRE_FILTER_SYMMETRY_COSINE,   // symmetry type of stage 2 filter
    { // Stage 1 filter coefficients
        39435,      329401,     1360560,    3599246,    6601764,    8388607,    6547850,    1001610,
        -4505541,   -5211008,   -621422,    4064305,    3502332,    -1242784,   -3894015,   -1166807,
        2855579,    2532881,    -1339935,   -2924582,   -80367,     2636097,    1146596,    -1970283,
        -1795151,   1163934,    2064146,    -376196,    -2034097,   -299771,    1794630,    819647,
        -1427924,   -1172210,   1001806,    1366909,    -568081,    -1424737,   163577,     1371999,
        187673,     -1236340,   -472107,    1044192,    684373,     -819180,    -825173,    581393,
        899460,     -347185,    -915049,    129183,     881489,     63537,      -809087,    -225092,
        708188,     352362,     -588665,    -444558,    459555,     502831,     -328750,    -529834,
        202807,     529247,     -86947,     -505444,    -14910,     463255,     100207,     -407601,
        -167645,    343169,     216913,     -274343,    -248603,    205072,     264118,     -138622,
        -265371,    77610,      254603,     -24054,     -234368,    -20767,     207269,     56265,
        -175733,    -82315,     142067,     99291,      -108348,    -108035,    76223,      109591,
        -47020,     -105189,    21756,      96258,      -966,       -84164,     -15138,     70169,
        26578,      -55507,     -33729,     41164,      37138,      -27899,     -37436,     16357,
        35525,      -6752,      -32266,     -997,       28572,      7574,       -25179,     -14901,
        21439,      27122,      -9639,      -47260,     -51810,     -31095,     -10524,     -1533
    },

    { // Stage 2 filter coefficients
        0,          0,          0,          0,          6917,       36010,      117457,     294022,
        601886,     1056422,    1624172,    2218368,    2720371,    3007535,    0,          0

    }
  }, //custom filter 1
  {
    SABRE_FILTER_SLOWROLLOFF,       // custom filter type
    SABRE_FILTER_SYMMETRY_COSINE,   // symmetry type of stage 2 filter
    { // Stage 1 filter coefficients
        0,          190799,     718147,     1804532,    3481898,    5536299,    7411221,    8388607,
        7873874,    5714951,    2416023,    -1000917,   -3378693,   -3938372,   -2688907,   -411446,
        1708498,    2659931,    2122733,    557874,     -1084853,   -1908449,   -1574818,   -414103,
        821794,     1417924,    1124432,    225950,     -678290,    -1054327,   -761721,    -67611,
        563184,     761258,     480642,     -38190,     -449978,    -522233,    -273035,    91351,
        335183,     332443,     134036,     -101185,    -229380,    -192459,    -50752,     84442,
        140173,     96992,      12276,      -56785,     -75761,     -39621,     493,        32962,
        38633,      17568,      -618,       -164,       62,         18,         -0,         0
    },

    { // Stage 2 filter coefficients
        6927430,    6927430,    0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0
    }
  }, //custom filter 2
  {
    SABRE_FILTER_FASTROLLOFF,       // custom filter type
    SABRE_FILTER_SYMMETRY_SINE,     // symmetry type of stage 2 filter
    { // Stage 1 filter coefficients
        -417,       1473,       3176,       1404,       -3339,      -2060,      4177,       3291,
        -5719,      -4716,      7588,       6691,       -10108,     -9048,      13208,      11986,
        -17155,     -15435,     22004,      19525,      -28016,     -24217,     35328,      29596,
        -44225,     -35626,     54922,      42351,      -67758,     -49730,     83039,      57778,
        -101197,    -66445,     122674,     75734,      -148069,    -85603,     178075,     96075,
        -213635,    -107173,    255963,     119032,     -306783,    -131888,    368551,     146257,
        -445016,    -163107,    542118,     184404,     -669965,    -214274,    847063,     262410,
        -1111089,   -355255,    1550405,    584903,     -2419106,   -1431913,   4642716,    8388607,
        4642716,    -1431913,   -2419106,   584903,     1550405,    -355255,    -1111089,   262410,
        847063,     -214274,    -669965,    184404,     542118,     -163107,    -445016,    146257,
        368551,     -131888,    -306783,    119032,     255963,     -107173,    -213635,    96075,
        178075,     -85603,     -148069,    75734,      122674,     -66445,     -101197,    57778,
        83039,      -49730,     -67758,     42351,      54922,      -35626,     -44225,     29596,
        35328,      -24217,     -28016,     19525,      22004,      -15435,     -17155,     11986,
        13208,      -9048,      -10108,     6691,       7588,       -4716,      -5719,      3291,
        4177,       -2060,      -3339,      1404,       3176,       1473,       -417,       0
    },

    { // Stage 2 filter coefficients
        0,          0,          0,          8997,       47076,      154163,     392557,     819728,
        1472724,    2329930,    3284770,    4177061,    4814439,    5043562,    0,          0
    }
  }, //custom filter 3
  {
    SABRE_FILTER_FASTROLLOFF,       // custom filter type
    SABRE_FILTER_SYMMETRY_SINE,     // symmetry type of stage 2 filter
    { // Stage 1 filter coefficients
        -3131,      -11380,     17068,      5059,       -21148,     -10470,     41391,      3177,
        -60665,     6542,       88359,      -29550,     -117174,    64953,      148119,     -119646,
        -174668,    195542,     193084,     -298418,    -194946,    432455,     171955,     -606753,
        -110716,    837093,     -7811,      -1163276,   220265,     1699543,    -606879,    -2904582,
        1311875,    8388607,    8388607,    1311875,    -2904582,   -606879,    1699543,    220265,
        -1163276,   -7811,      837093,     -110716,    -606753,    171955,     432455,     -194946,
        -298418,    193084,     195542,     -174668,    -119646,    148119,     64953,      -117174,
        -29550,     88359,      6542,       -60665,     3177,       41391,      -10470,     -21148,
        5059,       17068,      -11380,     -3131,      0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0,
        0,          0,          0,          0,          0,          0,          0,          0
    },

    { // Stage 2 filter coefficients
        0,          0,          0,          0,          0,          0,          99386,      355200,
        880080,     1746102,    2801046,    3902774,    4736866,    5017414,    0,          0
    }
  } //custom filter 1
};


#endif  /* End of   __ES9218P_H__   */

