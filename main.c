#include "basicController.h"

static const char programa1[] =
        "10 print \"Hola Mundo\"\n\
20 i=8 \n\
30 e=20 \n\
40 print i+e\n\
50 print e-i\n\
60 print 20/10\n\
70 print e\%i\n\
";

static const char programa2[] =
        "10 goto 50\n\
15 print \"Hola\"\n\
16 goto 70\n\
20 goto 40\n\
30 goto 60\n\
40 goto 30\n\
50 goto 20\n\
60 let c = 108\n\
61 print c\n\
62 goto 15\n\
70 end\n";

static const char programa3[] =
        "10 gosub 100\n\
20 for i = 1 to 10\n\
30 print i\n\
40 next i\n\
50 print \"end\"\n\
60 end\n\
100 print \"subRutina\"\n\
110 return\n";

static const char programa4[] =
        "10 print \"Bienvenidos \"\n\
20 s=10\n\
30 if s=10 then gosub 55\n\
40 for i=1 to s\n\
45 print i \n\
50 next i\n\
55 print \"SubRutina\"\n\
60 end\n\
";

static const char programa5[] =
        "10 print \"Bienvenidos \"\n\
20 s=30\n\
30 if s=10 then gosub 55 else s=5\n\
40 for i=1 to s\n\
45 print i \n\
50 next i\n\
55 print \"SubRutina\"\n\
60 end\n\
";

int main(void) {
    basic_init(programa1);

    do {
        basic_run();
    } while (!basic_finished());

    return 0;
}
