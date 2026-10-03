// Reads "<rule>\t<unix time>\t<offset east, s>" lines, prints the ones tzrule gets wrong.
#include "../../src/tzrule.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char line[256];
    long n = 0, bad = 0, unparsed = 0;
    while (fgets(line, sizeof line, stdin)) {
        char *rule = strtok(line, "\t"), *ts = strtok(NULL, "\t"), *want = strtok(NULL, "\t\n");
        if (!rule || !ts || !want) continue;
        tzrule_t r;
        if (!tzrule_parse(rule, &r)) { unparsed++; continue; }
        int32_t got = tzrule_offset(&r, atoll(ts));
        n++;
        if (got != atol(want) && bad++ < 15) printf("WRONG %s at %s: %d, want %s\n", rule, ts, got, want);
    }
    printf("%ld checked, %ld wrong, %ld rules not parsed\n", n, bad, unparsed);
    return bad != 0;
}
