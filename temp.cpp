#include <iostream>
#include "RadioLib_Ranging_Master/rangingCorrection.h"

using namespace std;

int main(int argc, char **argv) {
    
    // printf("const double rngCorrection[3][6] =\n{");
    // for (int k = 0; k < 3; k++) {
    //     printf("  {\n");
    //     for (int l = 0; l < 6; l++) {
    //         printf("    {\n");
    //         for (int i = 0; i < 16; i++) {
    //             printf("      ");
    //             for (int j = 0; j < 10; j++) {
    //                 printf("%.6lf", RangingCorrectionPerSfBwGain[l][k][i * 10 + j]);
    //                 if (!(i == 15 & j == 9))
    //                     printf(", ");
    //             }
    //             printf("\n");
    //         }
    //         printf("    },\n");
    //     }
    //     printf("},\n");
    // }
    // printf("}");

    // printf("%d %d", argv[1][0] - '0', argv[2][0] - '0');

    for (int i = 0; i < 160; i++) {
        printf("%lf\n", RNG_LUT[argv[1][0] - '0'][argv[2][0] - '0'][i]);
    }

    return 0;
}
