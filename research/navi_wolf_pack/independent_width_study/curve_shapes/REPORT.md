# Aligned independent Hall-curve morphology

This study aligns **945** raw Hall curves from the corrected October-15 independent set. Each curve runs from observed baseline departure to observed baseline return; its preceding observed ground mean is subtracted, the larger excursion is oriented positive, and its amplitude is normalized. No RVM window or RVM acceptance is used.

## Template test

The first five principal components explain 48.2%, 31.5%, 3.4%, 1.5%, 1.1% of shape variance. The one-template per-point squared-error distribution is median 0.0034 (5th–95th 0.0010–0.0131). When each curve is reconstructed from a leave-one-out version of that template, the median is 0.0034.

## Gaussian comparison

For context, each normalized curve was also fit individually to the proposed **line + one Gaussian lobe** model. Its median per-point squared error is 0.0007 (5th–95th 0.0004–0.0014; failed fits 0). This is not a BIC contest: the Gaussian is refit with five parameters for every curve, whereas the empirical template is learned from other curves and fixed at application time. It answers the narrower question of whether the measured forms are close enough to one reusable shape to justify testing it as an alternative recognizer model.

## Independent-boot check

- Holding out boot `15549e9385faa4d5` (n=883), the template learned from the other boot has median error 0.0040 (5th–95th 0.0009–0.0160).
- Holding out boot `4d8083de79a0e299` (n=62), the template learned from the other boot has median error 0.0029 (5th–95th 0.0011–0.0148).

| k | mean squared error | silhouette | cluster sizes |
|---:|---:|---:|---:|
| 1 | 0.00487 | — | [945] |
| 2 | 0.00342 | 0.262 | [476, 469] |
| 3 | 0.00276 | 0.261 | [169, 418, 358] |
| 4 | 0.00242 | 0.228 | [118, 234, 402, 191] |
| 5 | 0.00212 | 0.199 | [184, 133, 213, 281, 134] |

## Two-cluster descriptive split

The k=2 split is descriptive only; a positive silhouette would indicate separation, not physical magnet families. Duration, distance, direction and polarity summaries are retained so any apparent split can be checked for context rather than mistaken for a universal template.
- Cluster 0 (n=476): duration median 0.260 s; IR-width median 54.03 mm; CW/CCW 338/138; oriented-positive original polarity 249/476.
- Cluster 1 (n=469): duration median 0.240 s; IR-width median 57.55 mm; CW/CCW 329/140; oriented-positive original polarity 235/469.

## Limit

The result tests whether normalized observed forms are compact or separable in this one Otto dataset. It cannot prove physical magnet identity, establish a universal shape family, or transfer automatically to Toby, another sensor height, or a different reference regime.
