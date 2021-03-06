/* ** por compatibilidad se omiten tildes **
================================================================================
TRABAJO PRACTICO 3 - System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================
definicion de funciones del scheduler
*/

#include "defines.h"
#include "sched.h"
#include "game.h"

/**
 * Number of scheduler ticks until a task switch is necessary.
 */
#define SCHEDULER_TASK_TICKS 1

/** Scheduler tick
 * Whenever this value reaches 0, the scheduler will change tasks, doing a
 * context switch if necessary. Should there be a single task requiring
 * attention, nothing shall be done.
 */
uint tick = 0;

/** Player switch
 * Stores which player's turn is it now. Its values are 0 and 1.
 */
uchar playerSwitcher = 0;

/** Pirate switch
 * Stores which pirate's turn is it now. Its values are between 0 and
 * MAX_CANT_PIRATAS_VIVOS - 1.
 */
ushort pirateSwitcherA = 0;
ushort pirateSwitcherB = 0;

/** Processes a single scheduler tick, returns the task that should be switched
 * to next.
 *
 * @return Task index in the gdt
 */
int scheduler_tick() {
    /* Next task to schedule
     *
     * Possible codes are:
     *  0 - Initial task
     *  1 - Idle task
     *  * - Pirate tasks
     *
     * Notice that if (current - 2) / MAX_CANT_PIRATAS_VIVOS == 0, then the task
     * belongs to player A. Otherwise, it belongs to player 2.
     */
    uchar current = 0;

    // Update the tick
    tick = (tick + 1) % SCHEDULER_TASK_TICKS;

    // If the game is paused is active, we just jump to the idle task
    if (game_is_paused()) {
        return 1 + GDT_IDX_TASKB_DESC;
    }

    /**
     * The algorithm runs in two stages, with the variable `found` deciding
     * which stage we're currently on. Namely:
     *
     * - Stage 0: we are testing the scheduled player. There are two possible
     *   outputs from this: either we find some task for this player to run, or
     *   we don't. In case we do, this is pretty simple: all we have to do is
     *   exit the scheduling stage. In case we don't, we pass on to Stage 1.
     *
     * - Stage 1: the player whose turn it was had no tasks to run, which means
     *   that the player looses the turn, and hence we have to schedule the other
     *   player. This stage checks whether the other player has any task to run.
     *   If he does, then we just switch to that one, otherwise, we run the idle
     *   task.
     */

    if (tick == 0) {
        uchar found = 0;

        while (found < 2) {
            jugador_t *p;
            ushort *i;
            ushort pirateSwitcher;

            switch (playerSwitcher) {
                case 0:
                    p = &jugadorA;
                    i = &pirateSwitcherA;
                    pirateSwitcher = pirateSwitcherA;
                    break;
                case 1:
                    p = &jugadorB;
                    i = &pirateSwitcherB;
                    pirateSwitcher = pirateSwitcherB;
                    break;
            }

            do {
                if (p->piratas[*i].exists) {
                    break;
                }

                *i = (*i + 1) % MAX_CANT_PIRATAS_VIVOS;
            } while (*i != pirateSwitcher);

            if (p->piratas[*i].exists) {
                // We found some existant task to switch to, so we set the
                // current task to this one.
                current = *i + playerSwitcher * MAX_CANT_PIRATAS_VIVOS + 2;
                // We flip the player so that in the next run, its the other
                // player's turn.
                playerSwitcher = BIT_FLIP(playerSwitcher, 0);
                // Select the next task to be scheduled during the next run.
                *i = (*i + 1) % MAX_CANT_PIRATAS_VIVOS;
                break;
            } else {
                if (found == 0) {
                    // We are at stage 0, so we're checking the first player,
                    // and no task was found for this player, hence, just switch
                    // both player and stage.
                    // We flip the player so that in the next run, its the other
                    // player's turn.
                    playerSwitcher = BIT_FLIP(playerSwitcher, 0);
                    // Select stage one
                    found = 1;
                } else {
                    // We are at stage 1, so we just ran checks for both players,
                    // and found that neither of them has any task to run. Hence,
                    // we switch to the idle task.
                    current = 1;
                    break;
                }
            }
        }

        // Convert the task to switch to into an index in our GDT.
        return current + GDT_IDX_TASKB_DESC;
    } else {
        return -1;
    }
}
