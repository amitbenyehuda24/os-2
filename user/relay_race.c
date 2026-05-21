#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_TEAMS 3
#define RUNNERS_PER_TEAM 5
#define TARGET_SCORE 30

int main(int argc, char *argv[]) {
  // Default favoritism is 50, but can be changed via command line argument
  int favoritism = 50;
  if (argc > 1) {
    favoritism = atoi(argv[1]);
  }

  printf("=========================================\n");
  printf("Starting Relay Race with Favoritism: %d%%\n", favoritism);
  printf("=========================================\n");

  // 1. Initialize the Israeli Lock and reset the scores [cite: 221, 246]
  int lock_id = israeli_create(favoritism);
  if (lock_id < 0) {
    printf("Error: Failed to create Israeli lock\n");
    exit(1);
  }
  reset_scores();

  // 2. Spawn the runners (child processes) [cite: 227-230]
  for (int t = 0; t < NUM_TEAMS; t++) {
    for (int r = 0; r < RUNNERS_PER_TEAM; r++) {
      
      int pid = fork();
      if (pid < 0) {
        printf("Error: fork failed\n");
        exit(1);
      }
      
      if (pid == 0) { // Child process (Runner)
        // Assign the runner to their team 
        setgid(t);
        
        // 3. The Runner's Loop 
        while (1) {
          // (a) Acquire the baton [cite: 235]
          israeli_acquire(lock_id);

          // Check if the race is already over (someone else won while we slept)
          int highest_score = 0;
          for(int i = 0; i < NUM_TEAMS; i++) {
            int s = get_score(i);
            if(s > highest_score) highest_score = s;
          }
          
          if (highest_score >= TARGET_SCORE) {
            israeli_release(lock_id);
            exit(0); // Go home, the race is over
          }

          // (b) Increase the team's score [cite: 236]
          int my_score = add_score(t, 1);

          // (c) Print the required message 
          printf("Runner %d (Team %d) acquired the baton\n", getpid(), t);
          printf("Team %d score = %d\n", t, my_score);

          // Check if my team just won the race! [cite: 243]
          if (my_score >= TARGET_SCORE) {
            printf("\n>>> TEAM %d WINS THE RACE! <<<\n\n", t);
            israeli_release(lock_id);
            exit(0);
          }

          // (d) Release the baton [cite: 241]
          israeli_release(lock_id);

          // (e) Sleep briefly to simulate running and let others play 
          sleep(5); 
        }
      }
    }
  }

  // 4. Parent process waits for all runners to finish the race
  for (int i = 0; i < NUM_TEAMS * RUNNERS_PER_TEAM; i++) {
    wait(0);
  }

  // 5. Cleanup
  israeli_destroy(lock_id);
  printf("Race Finished Gracefully.\n");
  exit(0);
}