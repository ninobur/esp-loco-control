# Test corpus manifest

Source: `~/Pi_Log_Downloads/*.nsr` (NSR1 captures, Oct 25–26 2026). Decoded by `extract_nsr.py` (CRC-checked, no repair). Template source (Oct 15 Otto, `aligned_curves.csv`) is not in any partition below.

| capture | loco | partition | dur s | native samples | CRC bad | serial gaps | ref magnets (expected) | scored s |
|---|---|---|---:|---:|---:|---:|---:|---:|
| `9950011_20261025_133357.nsr` | Otto | level1 | 96.8 | 483936 | 0 | 0 | 20 (0) | 26.4 |
| `navi_session_9950011_1101C8E84BD203CE_CAC98672_20261025_185713.nsr` | Otto | dev | 396.3 | 1981680 | 0 | 0 | 175 (0) | 257.3 |
| `navi_session_9950011_278F53E8D6A6A152_863442F4_20261026_130108.nsr` | Otto | validation | 128.1 | 640704 | 0 | 0 | 11 (0) | 23.9 |
| `navi_session_9950011_2DBF9301D441406A_EFB1DB70_20261026_125003.nsr` | Otto | validation | 349.4 | 1747104 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950011_396E722E9B21858E_2810F463_20261025_143214.nsr` | Otto | dev | 424.5 | 2122368 | 0 | 0 | 167 (0) | 231.7 |
| `navi_session_9950011_39942C7C86D977F4_F5327735_20261025_144532.nsr` | Otto | dev | 292.6 | 1462800 | 0 | 2 | 168 (0) | 229.0 |
| `navi_session_9950011_4B4D99C1FC8A2402_E341D221_20261025_144039.nsr` | Otto | dev | 285.8 | 1428960 | 0 | 0 | 174 (0) | 244.9 |
| `navi_session_9950011_54CB76FF31D2533C_93C00FA1_20261026_112038.nsr` | Otto | validation | 239.0 | 1195008 | 0 | 0 | 41 (0) | 110.7 |
| `navi_session_9950011_5EB1E25967EA0D9F_C3650F9F_20261025_191440.nsr` | Otto | dev | 467.9 | 2336688 | 0 | 2 | 174 (0) | 287.3 |
| `navi_session_9950011_63DD5C95E06D134A_29B54108_20261025_183301.nsr` | Otto | dev | 528.9 | 2644368 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950011_7463C9467F2273E6_8DBC960E_20261025_151449.nsr` | Otto | dev | 553.7 | 2768592 | 0 | 0 | 365 (0) | 496.6 |
| `navi_session_9950011_77E1D0EB6602E52F_3FF447A6_20261026_131105.nsr` | Otto | validation | 292.7 | 1462416 | 0 | 1 | 176 (6) | 214.6 |
| `navi_session_9950011_7A62A59107547EAE_11DAC74E_20261025_185700.nsr` | Otto | excluded (<50k samples) | 2.3 | 11616 | 0 | 0 | — | — |
| `navi_session_9950011_8139C11783F1F6A0_9EB7A3B3_20261025_150002.nsr` | Otto | dev | 388.6 | 1943040 | 0 | 0 | 176 (0) | 305.8 |
| `navi_session_9950011_8B9F1FABA221C52D_FB2C11B8_20261025_195900.nsr` | Otto | dev | 30.5 | 152304 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950011_AEA377BE8702355B_DE880485_20261025_200053.nsr` | Otto | dev | 110.2 | 551040 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950011_C2C3E51F16AFA083_E9253DED_20261025_200750.nsr` | Otto | dev | 422.6 | 2112864 | 0 | 0 | 174 (0) | 253.9 |
| `navi_session_9950011_D1EC9A0B0A105CD2_B5B48C5A_20261025_141344.nsr` | Otto | dev | 334.0 | 1669776 | 0 | 0 | 146 (6) | 147.6 |
| `navi_session_9950011_D29E200C39A80EC7_CDCCADA8_20261026_113500.nsr` | Otto | validation | 192.4 | 961776 | 0 | 0 | 73 (0) | 106.4 |
| `navi_session_9950011_F420A05DCE376BCB_2C88DC27_20261026_114657.nsr` | Otto | validation | 2286.6 | 11433168 | 0 | 0 | 1707 (65) | 2194.7 |
| `navi_session_9950011_FBD13DC1F568014F_F59AA401_20261025_193740.nsr` | Otto | dev | 146.5 | 732384 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950012_06CFF492E36BFCB0_85C18893_20261026_101548.nsr` | Toby | transfer | 183.6 | 917808 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950012_0D92C1805255ECAE_6CDDD0D5_20261026_094944.nsr` | Toby | transfer | 195.7 | 978480 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950012_22BBF8FCFE386F3D_D53310BE_20261026_102503.nsr` | Toby | transfer | 3202.6 | 16001616 | 0 | 10 | 718 (0) | 1013.8 |
| `navi_session_9950012_2623C224035307E9_7AD84649_20261025_180656.nsr` | Toby | transfer | 313.5 | 1567536 | 0 | 0 | 190 (0) | 257.7 |
| `navi_session_9950012_2DC048162F8C579F_8323322C_20261025_185727.nsr` | Toby | transfer | 384.4 | 1922208 | 0 | 0 | 175 (0) | 257.4 |
| `navi_session_9950012_5F2005DA1FBBC01E_187A1D30_20261025_191436.nsr` | Toby | transfer | 473.8 | 2365824 | 0 | 3 | 173 (0) | 284.9 |
| `navi_session_9950012_799E9E058F1382BD_E89FE745_20261026_095302.nsr` | Toby | transfer | 321.2 | 1606224 | 0 | 0 | 171 (0) | 259.4 |
| `navi_session_9950012_83100720D44BDAB1_E7B4D9E7_20261026_102433.nsr` | Toby | transfer | 27.4 | 137184 | 0 | 0 | 0 (0) | 0 |
| `navi_session_9950012_8CFE3CE1CCE52D3D_164FAB31_20261026_122609.nsr` | Toby | transfer | 1019.4 | 5090736 | 0 | 12 | 686 (24) | 949.4 |
| `navi_session_9950012_9D8C9C19279A29AD_4D1E66C8_20261025_184241.nsr` | Toby | transfer | 371.8 | 1858944 | 0 | 0 | 164 (0) | 263.1 |
| `navi_session_9950012_9E37ADE2EFB0E0D7_F3826809_20261025_175823.nsr` | Toby | transfer | 25.1 | 97776 | 0 | 53 | 0 (0) | 0 |
| `navi_session_9950012_B695139999A7B587_BB2D5C01_20261025_180046.nsr` | Toby | excluded (<50k samples) | 10.4 | 39696 | 0 | 26 | — | — |
| `navi_session_9950012_C354E790319577EA_58DAC53F_20261026_100013.nsr` | Toby | transfer | 369.1 | 1845552 | 0 | 0 | 185 (0) | 318.1 |
| `navi_session_9950012_D3B52A95255F8497_4A69B912_20261026_100832.nsr` | Toby | transfer | 319.8 | 1598976 | 0 | 0 | 177 (0) | 256.9 |
| `navi_session_9950012_D5F9CF8FD06D12F3_12EC5BB1_20261026_101853.nsr` | Toby | excluded (<50k samples) | 0.5 | 2448 | 0 | 0 | — | — |
| `navi_session_9950012_DD9FE96B6F09252E_D1B726B4_20261026_101855.nsr` | Toby | excluded (<50k samples) | 3.1 | 15360 | 0 | 0 | — | — |
| `navi_session_9950012_E7C3380915945FB0_FCB1031B_20261026_125212.nsr` | Toby | transfer | 1770.0 | 8849856 | 0 | 0 | 1206 (42) | 1551.7 |
| `navi_session_9950012_EC79FDC0E8543B1E_21B8A1C4_20261025_180129.nsr` | Toby | transfer | 299.2 | 1487664 | 0 | 59 | 174 (0) | 230.9 |

SHA-256 of every capture is in `CORPUS_MANIFEST.json`.

## Known limitations

- The reference is route-anchored oracle evidence (see `truth.py`), not physical ground truth. It is weakest where the background steps or bursts (D1EC 64–76 s and 176–192 s, F420 39–100 s, 278F, 54CB).
- Native `direction` is 0 throughout `54CB76FF…` although the loco moved; both directions were tried there.
- `278F53E8…` contains wide flat-topped excursions (slow crawl) beyond the 1.5 s top scale.
- From Oct 26, MM066, MM067 and MM069 read with the polarity opposite to RouteMap.h on both locos.
- IR records were not decoded; no IR-derived speed is used anywhere.
- Toby (`transfer`) has no template of his own; the Otto Oct-15 template is applied unchanged.
