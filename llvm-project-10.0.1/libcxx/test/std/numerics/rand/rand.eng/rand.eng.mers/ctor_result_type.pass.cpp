//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <random>

// template <class UIntType, size_t w, size_t n, size_t m, size_t r,
//           UIntType a, size_t u, UIntType d, size_t s,
//           UIntType b, size_t t, UIntType c, size_t l, UIntType f>
// class mersenne_twister_engine;

// explicit mersenne_twister_engine(result_type s = default_seed);

#include <random>
#include <sstream>
#include <cassert>

#include "test_macros.h"

void
test1()
{
    const char* a = "0 1 1812433255 1900727105 1208447044 2481403966 4042607538 337614300 "
    "3232553940 1018809052 3202401494 1775180719 3192392114 594215549 184016991 "
    "829906058 610491522 3879932251 3139825610 297902587 4075895579 2943625357 "
    "3530655617 1423771745 2135928312 2891506774 1066338622 135451537 933040465 "
    "2759011858 2273819758 3545703099 2516396728 1272276355 3172048492 "
    "3267256201 2332199830 1975469449 392443598 1132453229 2900699076 "
    "1998300999 3847713992 512669506 1227792182 1629110240 112303347 2142631694 "
    "3647635483 1715036585 2508091258 1355887243 1884998310 3906360088 "
    "952450269 3647883368 3962623343 3077504981 2023096077 3791588343 "
    "3937487744 3455116780 1218485897 1374508007 2815569918 1367263917 "
    "472908318 2263147545 1461547499 4126813079 2383504810 64750479 2963140275 "
    "1709368606 4143643781 835933993 1881494649 674663333 2076403047 858036109 "
    "1667579889 1706666497 607785554 1995775149 1941986352 3448871082 "
    "2109910019 1474883361 1623095288 1831376534 2612738285 81681830 2204289242 "
    "1365038485 251164610 4268495337 1805601714 1262528768 1442526919 "
    "1675006593 965627108 646339161 499795587 840887574 380522518 3023789847 "
    "1457635507 1947093157 2600365344 2729853143 1550618999 1390905853 "
    "3021294812 882647559 838872117 1663880796 4222103589 2754172275 3844026123 "
    "3199260319 4176064873 3591027019 2690294242 2978135515 3172796441 "
    "3263669796 1451257057 1427035359 4174826006 2171992010 1537002090 "
    "3122405306 4162452508 3271954368 3794310005 3240514581 1270412086 "
    "3030475836 2281945856 2644171349 3109139423 4253563838 1289926431 "
    "1396919653 733220100 2753316645 1196225013 3699575255 3569440056 "
    "2675979228 2624079148 3463113149 863430286 623703199 2113837653 2656425919 "
    "175981357 4271478366 4238022735 1665483419 86880610 2963435083 1830392943 "
    "847801865 3237296945 332143967 3973606945 2671879697 2236330279 2360127810 "
    "3283955434 203240344 4048139172 13189264 2263058814 247241371 1566765783 "
    "3084408095 3719371299 1958375251 1985924622 1712739232 1861691451 "
    "2644502937 2337807839 784993770 2962208780 2190810177 1523122731 "
    "714888527 578678761 3698481324 1801168075 534650483 3390213921 3923356461 "
    "3586009066 2059432114 52511333 1969897376 3630122061 524661135 3513619765 "
    "563070233 501359785 477489274 658768624 938973567 1548584683 1345287459 "
    "2488691004 3441144905 3849305094 2430000078 855172178 614463281 2092744749 "
    "176381493 1655802051 2273888101 2474494847 3471978030 2138918303 575352373 "
    "1658230985 1675972553 2946663114 915579339 284981499 53939948 3022598146 "
    "1861218535 3403620774 4203516930 2360471119 3134536268 1383448498 "
    "1307602316 3847663247 3027225131 3597251613 3186237127 725127595 "
    "1928526954 1843386923 3560410503 54688266 1791983849 2519860352 4256389699 "
    "2328812602 486464275 3578698363 301279829 1303654791 4181868765 971794070 "
    "1933885487 3996807464 2144053754 4079903755 3775774765 3481760044 "
    "1212862354 1067356423 3764189132 1609862325 2209601551 2565747501 "
    "161962392 4045451782 2605574664 2520953090 3490240017 1082791980 44474324 "
    "101811128 4268650669 4171338684 772375154 3920460306 2319139534 599033750 "
    "2950874441 3373922995 1496848525 4095253594 1271943484 1498723121 "
    "3097453329 3698082465 281869581 3148270661 3591477288 747441437 2809508504 "
    "3896107498 303747862 2368081624 1844217645 886825352 287949781 1444561207 "
    "2512101757 2062331723 741720931 1383797313 3876746355 2041045348 "
    "2627599118 1124169970 200524822 3484820454 55883666 1135054804 669498692 "
    "2677215504 3097911127 1509628615 617580381 2229022193 85601568 3243896546 "
    "3715672328 912168347 2359163500 1180347564 4243175048 2092067103 880183327 "
    "4000664709 2045044777 3500474644 1515175520 1862207123 186628841 "
    "3337252925 708933575 4015964629 3136815297 3314919747 2891909013 "
    "3316567785 3944275369 3608506218 2884839110 3054055598 2707439927 "
    "1381111877 3275487281 4292456216 2639563270 3327301876 3576924628 "
    "721056309 2002808140 748967365 52380958 2200261692 763456477 1708381337 "
    "2038446433 2682979402 1526413779 2211263302 3879771969 75966584 3645059271 "
    "2985763524 4085690255 82390958 1883631385 1647521260 1598026998 3038041577 "
    "2501913134 3279302868 1738888524 805035483 756399074 3863810982 1097797270 "
    "1505792529 898904527 583561003 717152376 3333867738 1099456544 1663473545 "
    "1242141229 3828627682 1966201676 1713552361 3852160017 1584965284 21695908 "
    "1013262144 145341901 3995441263 3462066219 2239637848 1214086163 "
    "2428868268 1650037305 1545513388 1621198806 4232947817 1823092073 "
    "256414624 1745018809 1357102386 2055139770 3280958307 2482431613 "
    "1664870585 859130423 4097751123 3079768369 2470211009 2984880786 "
    "2808568948 2877071923 1984903163 302768457 1866396789 869566317 3746415787 "
    "4169433075 3025005404 3980733379 3539207278 3953071536 876960847 "
    "2548872156 800507464 1865466907 1273317878 3754712872 1757188269 "
    "3229950355 3731640200 2283390608 2204990292 411873449 447423849 1852437802 "
    "472825525 3044219944 2913114194 1859709265 4053786194 574820536 2104496732 "
    "865469814 2438352724 4208743605 4215067542 1364015250 4139974345 "
    "3838747005 1818502786 2914274940 1402365828 1751123528 2302578077 "
    "2463168652 1968705496 1730700144 3023943273 1139096844 2658667767 "
    "2063547264 705791165 1444775274 2415454225 1575664730 921044163 648101324 "
    "1212387162 4191962054 1787702169 1888718041 1518218010 3398792842 "
    "4079359729 149721439 750400353 2661036076 3802767886 520152586 951852508 "
    "2939585975 1375969109 385733137 3523607459 1902438415 4250996086 "
    "2712727066 484493674 3932107461 1428488210 1764242548 3424801055 "
    "4004904451 2226862072 2393366939 3609584727 3614444319 317349896 "
    "3826527525 204023804 981902443 3356042039 3051207045 1869902661 561831895 "
    "3706675415 1527687593 1227610446 2596341042 3191717368 3269246891 "
    "557877074 4062070629 3052520266 3772487029 400039836 3195205275 4085394797 "
    "1655557239 1345770144 2864727192 449281238 73189507 528365765 2727400656 "
    "247880434 2408277395 777039183 2210179398 1088433648 2124356402 1555630141 "
    "604790219 195012151 3312518356 923728373 3999251660 3313059535 3478133921 "
    "3395026960 383464614 3425869222 2446885186 4032184426 157195416 3158909476 "
    "1663750443 2046427584 1658453076 1784483001 3146546889 1238739785 "
    "2297306523 3472330897 2953326031 2421672215 1221694592 1588568605 "
    "2546987845 3375168573 2137961649 3056565164 330165219 235900365 1000384800 "
    "2697255904 579122283 3050664825 73426122 1232986102 2940571064 3076486824 "
    "1708182873 2796363264 292154131 4280019913 1102652157 1185393592 "
    "1494991690 4270076389 2384840717 425785147 2385321880 317514772 3926962743 "
    "392176856 3465421709 1878853468 122662664 2958252160 1858961315 2244939588 "
    "2361884409 2860936803 683833250 3291277128 1686857206 1112632275 "
    "1200680507 3342928196 2677058150 939442136 3407104669 2906783932 "
    "3668048733 2030009470 1910839172 1234925283 3575831445 123595418 "
    "2362440495 3048484911 1796872496";
    std::mt19937 e1(0);
    std::ostringstream os;
    os << e1;
    assert(os.str() == a);
}

void
test2()
{
    const char* a = "0 1 6364136223846793007 13885033948157127961 "
    "15324573939901584278 12737837167382305846 15195339788985155882 "
    "6554113247712070460 17235932740818599105 13007415075556305955 "
    "6585479514541334743 8274505865835507625 1718218088692873364 "
    "10390651247454232081 12911994993614796389 3986900029987203370 "
    "6673827651897561714 4426752746717694792 7419158062930293690 "
    "5800047417539173618 15710773105226458059 16164512590413496893 "
    "3438015953120274172 3483801391287623267 293704481016263807 "
    "11580856846363212652 3489109114147636336 3391036861618673611 "
    "8265793309278544843 7557898467821912223 11008748280761875940 "
    "15929443707841919885 8545695347411085846 10810459396490399532 "
    "12233244910455127352 15556950738631379285 16711543556686614082 "
    "12362193084052127890 16520645558585805174 5163125267185202360 "
    "405552980610370477 17567412011316060306 18195950784827697319 "
    "7893142112162906367 11294475722810961618 7284845498332539581 "
    "8406882439401998138 4375387785957411470 9627875716250684710 "
    "8860968026642934661 9743109216691708518 152611520104818631 "
    "5897430410700879663 5351672954305365323 16325991383734641720 "
    "9695181037355459478 15420132328343498044 17772146581546890572 "
    "12095930435311226909 3066005052193896110 11579395203346116306 "
    "9168946227698330317 18318927644793076250 16096433325093805476 "
    "14945900876820434843 16826760579960858105 17664119339058295604 "
    "17844797344364136942 1071414400549507565 16688779616725465582 "
    "3684635020921274863 12774052823168116810 17270284502989966576 "
    "1081012692742984704 4377021575203177546 18341292555997099853 "
    "13297939683513494274 15065725504474304333 10796297883750572804 "
    "15233335271871291997 8767977593699151062 3360856014170688284 "
    "7828232912764786750 15167717223619970150 9622174963375022357 "
    "18262792478991268448 1196631425707106493 5368342740538672272 "
    "10381091599850241237 12108437846306626340 6150860188928778248 "
    "3342980288459577584 12715439159457051276 17996971042887275859 "
    "9749679821487730542 17763727344608586331 16024467606290078168 "
    "7763401887585513696 4448278918811662063 16947956613780322662 "
    "15144807360840708645 3878082612940188435 10709780449699561405 "
    "1649555943517500922 3206645931693769562 12562913950237146427 "
    "237213339742767727 12987800257476421358 1653669854585203688 "
    "3485900643226898485 13961759114404652223 5243794832751327611 "
    "10337687908642742498 16946139522050041809 16716562961992396380 "
    "4275124606042261542 4055100795824867618 6424268654905981295 "
    "3424516503413156556 2670380025813203539 10750762735193959951 "
    "8790031149370411970 4021216986392972993 12076090355041998696 "
    "14407920322903159838 10653597737935867030 15483225617438352002 "
    "2497775263858626604 12295882369431088188 14256043521530136935 "
    "2687322778627883798 3419797801078863201 8786888481486602641 "
    "445698423634900693 9597067954623467255 7101345576557603992 "
    "1498579197046783597 10403325187679734962 2464586980321053562 "
    "2022012026329844477 10802281218030350853 6628929099856200904 "
    "6828177972863192803 8589868113309334601 5245595233272009016 "
    "5335692004673212054 4515133017699498525 15966447436053813932 "
    "15199779177078162007 4190689609934804313 13003438276435994683 "
    "8406046831313066396 10564320513686955057 12668913223662201488 "
    "13130110932487580228 1030848205404711145 17684061609212954769 "
    "12942207438298787911 10731611242140874687 5165052527778107352 "
    "16323046249518133445 17119162873327029615 5754858052433703070 "
    "3864761150247579030 9945988334920003074 11409854727071782565 "
    "5000838138362434817 15526574143469400487 18094554078711846524 "
    "5576294272011007484 3478525338408894755 11392694223389544658 "
    "4692963068671452476 4459301637730340710 9699395817392066460 "
    "14644636990626292085 18065377773424192622 5217202490849387226 "
    "16175595974171756081 2109372019589047677 1624752883142646445 "
    "13462209973053735966 12082930933973802402 1568864426788967895 "
    "17047994306870001795 10556833209957537593 955604103878351641 "
    "9062985603395234592 9757612676622840969 1767246562613391916 "
    "9752598821733361274 7499745701633625047 7824811626141302622 "
    "15819064077972391284 5660565551854829485 17645390577243129343 "
    "7343780801046581776 2233358068547689666 8716657172695403744 "
    "9129027798969787220 334709674395230649 2063182499026924878 "
    "13089071159640936832 1765917316143960741 17552378408917656269 "
    "3917018959478722819 15626740210483166037 1645962609209923821 "
    "12277169606472643961 14545894350924442736 11485249378718653961 "
    "9205208816702766530 10967561305613932827 3105992977398681914 "
    "2125140299311648264 11619505070220308543 5030167448920096401 "
    "4248170446421798953 16184577688118775567 9240607582885304823 "
    "11838996733938359277 415426114101983968 14340734742548675134 "
    "4124085748228276376 17686494750190224280 9472996569628985376 "
    "1207013222233148636 3031046462562068367 45068538181330439 "
    "8678647417835301152 10693327126492781235 3058899219097846020 "
    "18377730418355022492 10269941972656742364 15986476992758938864 "
    "14575856764093007010 14749682846448398393 1042926396621439263 "
    "12184905641567868001 3518848236485931862 6718580865438347534 "
    "6319395993260741012 2855168874111910691 2482146419106261786 "
    "17290238774162848390 8071397865265268054 15873003794708011585 "
    "14422764926380465297 14140979091525022882 3573480287238168646 "
    "1525896111244666696 7537826952717918371 10482908122538761078 "
    "17003233041653857 9473838740924911883 8438240966750123668 "
    "10697754962581554225 15048771265786776312 9067877678399943713 "
    "3399555692325948067 6150260207049997483 7165140289246675175 "
    "14816202987105583988 4753550992948864498 10549400354582510015 "
    "13212062554023586370 1585477630313819722 476999696494664205 "
    "3208739183359199317 16011681780347380478 8149150693959772807 "
    "803412833228911773 2360961455092949929 1517204230639934662 "
    "13863717544358808032 16792122738584967930 12742474971940259289 "
    "1859755681395355028 1540431035241590810 3883807554140904361 "
    "16189061625447625933 12376367376041900879 8006563585771266211 "
    "2682617767291542421 8593924529704142157 9070391845133329273 "
    "3557484410564396342 9301398051805853085 12632514768298643219 "
    "227653509634201118 7247795074312488552 4939136716843120792 "
    "6533591052147596041 1308401457054431629 17488144120120152559 "
    "14075631579093810083 4015705597302725704 6833920126528811473 "
    "5095302940809114298 8312250184258072842 15770605014574863643 "
    "14091690436120485477 15763282477731738396 16394237160547425954 "
    "5066318118328746621 13140493775100916989 6371148952471982853 "
    "15150289760867914983 4931341382074091848 12635920082410445322 "
    "8498109357807439006 14836776625250834986";
    std::mt19937_64 e1(0);
    std::ostringstream os;
    os << e1;
    assert(os.str() == a);
}

int main(int, char**)
{
    test1();
    test2();

  return 0;
}
