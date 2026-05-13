#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  printf("Starting PRNG test...\n");

  // Set an initial seed
  lcg_srand(42);
  printf("Seed set to 42.\n");

  // Generate and print 5 pseudo-random numbers
  printf("Generating 5 numbers:\n");
  for(int i = 0; i < 5; i++) {
    uint random_num = lcg_rand();
    printf("Number %d: %d\n", i + 1, random_num);
  }

  // Change the seed and test again to see the sequence changes
  printf("\nChanging seed to 100...\n");
  lcg_srand(100);

  for(int i = 0; i < 3; i++) {
    uint random_num = lcg_rand();
    printf("Number %d: %d\n", i + 1, random_num);
  }

  printf("Test completed successfully!\n");
  exit(0);
}