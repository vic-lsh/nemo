#include "fair-share.h"

#include <sys/time.h>

#include "core-util.h"
#include "migration.h"
#include "mm-async.h"
#include "mm.h"
#include "physmem/config.h"
#include "policy.h"
#include "proc-mgr.h"
#include "ucm-config.h"
#include "ucm.h"
#include "util/compiler.h"

void fair_share_policy_epoch(void *opaque, struct policy_opts *opts) {
    UNUSED(opaque);
    UNUSED(opts);

    uint64_t migrate_down_bytes;
    struct hemem_process *process, *tmp;
    struct timeval now;
    int num_need_memory;
    int num_take_memory;
    double memshare_need;
    double memshare_take;
    uint64_t delta_need;
    uint64_t delta_take;
    struct hemem_process *need_fastmem[MAX_PROCESSES];
    struct hemem_process *take_fastmem[MAX_PROCESSES];
    double max_ratio = 100.0;
    uint64_t interprocess_migrate = PEBS_MIGRATE_RATE / 2;
    int64_t dram_list_cnts[NUM_HOTNESS_LEVELS];
    int64_t nvm_hot_pages_left, pages_from_cur_dram;
    uint64_t migrate_share;
    int i;
    int j;
    uint64_t memshare_proc;
    double delta_away;
    bool migrate_down_flag = true;

    num_need_memory = 0;
    num_take_memory = 0;
    memshare_proc = 0;
    memshare_need = 0;
    memshare_take = 0;
    delta_need = 0;
    delta_take = 0;
    delta_away = 0;
    memshare_proc = 0;

    process = peek_process(&processes_list);
    while (process != NULL) {
        pthread_mutex_lock(&(process->process_lock));
        mm_drain_async_requests(process);

        struct process_policy *policy = &process->policy;

        if (policy->access_count[FASTMEM] + policy->access_count[SLOWMEM] !=
            0) {
            if (policy->current_miss_ratio == -1) {
                // first time we have actual data to compute, but don't want
                // to include the -1.0 values in the EWMA, so just comute a
                // raw miss ratio here
                policy->current_miss_ratio =
                    process_policy_calc_miss_ratio(policy);
            } else {
                double miss_ratio = process_policy_calc_miss_ratio(policy);
                policy->current_miss_ratio =
                    (EWMA_FRAC * miss_ratio) +
                    ((1 - EWMA_FRAC) * policy->current_miss_ratio);
            }
            policy->access_count[FASTMEM] = 0;
            policy->access_count[SLOWMEM] = 0;
            process->still_migrating = false;
        } else {
            // we use a negative current miss ratio to signal that we don't
            // have any access information for this process yet, so rest of
            // policy thread shouldn't try to manage it for now
            policy->current_miss_ratio = 0;
            process->still_migrating = true;
        }

        memshare_proc += process->mm.current_fastmem;

        for (int xxx = LAST_HEMEM_THREAD + 1; xxx < HEMEM_NCORES; xxx++) {
            policy->sample_count[xxx] = 0;
        }

        policy->deviation_ratio =
            policy->current_miss_ratio / policy->target_miss_ratio;
        // policy->ratio = policy->target_miss_ratio -
        // policy->current_miss_ratio;
        delta_away += policy->deviation_ratio;
        tmp = process;
        process = process->next;
        pthread_mutex_unlock(&(tmp->process_lock));
    }

    delta_away /= processes_list.numentries;
    // LOG("ratio : %f\tDelta_AWAY : %f\n",policy->ratio,delta_away);
    process = peek_process(&processes_list);
    while (process != NULL) {
        pthread_mutex_lock(&(process->process_lock));

        struct process_policy *policy = &process->policy;

        if (policy->deviation_ratio > delta_away) {
            need_fastmem[num_need_memory] = process;
            num_need_memory++;
            if (policy->deviation_ratio > max_ratio) {
                // clip ratio to some maximum, here the interprocess migrate
                // rate
                // TODO: Is this the right thing to do? The ratio could
                // potentially be infinite
                policy->deviation_ratio = max_ratio;
            }
            memshare_need += policy->deviation_ratio;

        } else if ((policy->deviation_ratio < delta_away) &&
                   (process->still_migrating == false)) {
            take_fastmem[num_take_memory] = process;
            num_take_memory++;
            policy->deviation_ratio =
                ((policy->target_miss_ratio / policy->current_miss_ratio));
            if (policy->deviation_ratio > max_ratio) {
                // clip ratio to some maximum, here the interprocess migrate
                // rate
                // TODO: Is this the right thing to do? The ratio could be
                // infinite particularly if the current miss ratio is 0
                policy->deviation_ratio = max_ratio;
            }
            memshare_take += policy->deviation_ratio;
        }

        tmp = process;
        process = process->next;
        pthread_mutex_unlock(&(tmp->process_lock));
    }

    for (i = 0; i < num_need_memory; i++) {
        // Compute policy->dram_delta here (>0)
        process = need_fastmem[i];
        pthread_mutex_lock(&(process->process_lock));

        struct process_policy *policy = &process->policy;

        // TODO: (process->atio / memshare_need) should be between 0 and 1,
        // right? given how it is computed above. Then dram_delta should be
        // <= interprocess_migrate
        assert((policy->deviation_ratio / memshare_need) <= 1);

        if (policy->fastmem_delta < 0) {
            // previously had surpluss dram last epoch and was in the
            // process of giving some up but now this process needs more
            // dram, so stop giving up dram
            policy->fastmem_delta = 0;
            policy->migration_down_bytes = 0;
        } else if (policy->fastmem_delta == 0) {
            // process needs more dram (first time)
            policy->migration_up_bytes =
                (policy->deviation_ratio / memshare_need) *
                (interprocess_migrate);
            policy->migration_up_bytes =
                (policy->migration_up_bytes > FAIR_SHARE_DRAM)
                    ? FAIR_SHARE_DRAM
                    : policy->migration_up_bytes;
            policy->fastmem_delta = FAIR_SHARE_DRAM;
        } else {  // > 0
            // process needed more dram last epoch, still needs more, so
            // continue giving it dram
            if (policy->fastmem_delta >
                ((policy->deviation_ratio / memshare_need) *
                 (interprocess_migrate))) {
                policy->migration_up_bytes =
                    (policy->deviation_ratio / memshare_need) *
                    (interprocess_migrate);
            } else {
                policy->migration_up_bytes = policy->fastmem_delta;
            }
        }

        // round down to hugepage size
        policy->migration_up_bytes -=
            (policy->migration_up_bytes % HUGEPAGE_SIZE);
        assert(policy->migration_up_bytes <= (interprocess_migrate));
        if (policy->migration_up_bytes + process->mm.current_fastmem >
            process->mm.mem_allocated) {
            // don't give out more dram than we need for this process
            policy->migration_up_bytes -=
                ((policy->migration_up_bytes + process->mm.current_fastmem) -
                 process->mm.mem_allocated);
        }
        delta_need += policy->migration_up_bytes;
        pthread_mutex_unlock(&(process->process_lock));
    }

    for (i = 0; i < num_take_memory; i++) {
        // Compute policy->dram_delta here (<0)
        process = take_fastmem[i];
        pthread_mutex_lock(&(process->process_lock));

        struct process_policy *policy = &process->policy;

        // TODO: (tmp_ratio / memshare_take) should be between 0 and 1,
        // right? given how it is computed above. Then dram_delta should be
        // <= interprocess_migrate
        assert((policy->deviation_ratio / memshare_take) <= 1);

        if (policy->fastmem_delta > 0) {
            // process was being given more dram, but now it needs to give
            // some up
            policy->fastmem_delta = 0;
            policy->migration_up_bytes = 0;
        } else if (policy->fastmem_delta == 0) {
            // process needs to give up fast mem (first time) -- comput MD
            // -- give up a quarter of dram allocation
            policy->migration_down_bytes =
                (policy->deviation_ratio / memshare_take) *
                (interprocess_migrate);
            policy->migration_down_bytes =
                (policy->migration_down_bytes >
                 (process->mm.current_fastmem / 4))
                    ? (process->mm.current_fastmem / 4)
                    : policy->migration_down_bytes;
            policy->fastmem_delta = (process->mm.current_fastmem / 4);
        } else {  // < 0
            // process still needs to give up fast mem from last epoch, so
            // keep doing that
            if ((-1 * policy->fastmem_delta) >
                ((policy->deviation_ratio / memshare_take) *
                 (interprocess_migrate))) {
                policy->migration_down_bytes =
                    (policy->deviation_ratio / memshare_take) *
                    (interprocess_migrate);
            } else {
                policy->migration_down_bytes = (-1 * policy->fastmem_delta);
            }

            policy->fastmem_delta *= -1;
        }

        // round down to hugepage size
        policy->migration_down_bytes -=
            (policy->migration_down_bytes % HUGEPAGE_SIZE);
        assert(policy->migration_down_bytes <= (interprocess_migrate));
        if (policy->migration_down_bytes > process->mm.current_fastmem) {
            // don't take away more dram than this process is allowed
            policy->migration_down_bytes = process->mm.current_fastmem;
        }
        delta_take += policy->migration_down_bytes;
        policy->fastmem_delta *= -1;
        assert(policy->fastmem_delta <= 0);
        // dram delta is negative if we are taking memory
        pthread_mutex_unlock(&(process->process_lock));
    }

    // can we satisfy memory needed with free dram?
    if ((get_fastmem_size() - memshare_proc) > delta_need) {
        // if so, don't take dram from processes
        migrate_down_flag = false;
    } else {
        migrate_down_flag = true;
    }

    gettimeofday(&now, NULL);

    // now, carry out the migrations
    process = peek_process(&processes_list);
    while (process != NULL) {
        pthread_mutex_lock(&(process->process_lock));

        struct process_policy *policy = &process->policy;

        if ((policy->fastmem_delta < 0) && migrate_down_flag) {
            process_migrate_down_bytes(process, policy->migration_down_bytes);
            policy->fastmem_delta *= -1;
            policy->fastmem_delta -= policy->migration_down_bytes;
            policy->fastmem_delta *= -1;
        } else if (policy->fastmem_delta > 0) {
            process_migrate_up_bytes(process, policy->migration_up_bytes);
            policy->fastmem_delta -= policy->migration_up_bytes;
        } else {
            // process has correct amount of DRAM, need to migrate down
            // enough pages to free dram for the hot NVM pages.

            // get number of pages that are in a hotter nvm list than a dram
            // list do we need a better way to do this with the hot lists?

            // for each hotness in NVM we ask: how many pages of DRAM are we
            // allowed to replace?
            for (i = 0; i < NUM_HOTNESS_LEVELS; i++) {
                dram_list_cnts[i] = process->mm.fastmem_lists[i].numentries;
            }
            migrate_down_bytes = 0;
            for (i = NUM_HOTNESS_LEVELS - 1; i > 2; i--) {
                // algo:
                // -for each NVM hotness we want to get how many pages we
                // can fit into
                //  DRAM if we swap colder pages
                // -dram_list_cnts is to prevent double counting.
                // 1) how many pages are at this NVM hotness.
                // 2) count how many DRAM pages are lower than this hotness
                // 3) repeat for each DRAM hotness
                nvm_hot_pages_left = process->mm.slowmem_lists[i].numentries;
                for (j = 0; j < i; j++) {
                    // if we got all the hot pages up then we stop checking
                    if (nvm_hot_pages_left <= 0) {
                        break;
                    }

                    // if this level of DRAM has no pages left bail.
                    if (dram_list_cnts[j] == 0) {
                        continue;
                    }

                    // pages we want from this DRAM level is min(pages at
                    // this DRAM level, pages we want to move up)
                    pages_from_cur_dram =
                        min(dram_list_cnts[j], nvm_hot_pages_left);
                    dram_list_cnts[j] -= pages_from_cur_dram;
                    assert(dram_list_cnts[j] >= 0);

                    // TODO: this will not be correct if pages are non-uniform
                    // in size.
                    migrate_down_bytes += pages_from_cur_dram * HUGEPAGE_SIZE;
                }
            }

            migrate_share = PEBS_MIGRATE_RATE / ((processes_list.numentries > 0)
                                                     ? processes_list.numentries
                                                     : 1);

            if (migrate_down_bytes > (migrate_share / 2)) {
                migrate_down_bytes = (migrate_share / 2);
            }
            policy->migration_down_bytes = migrate_down_bytes;
            policy->migration_up_bytes = policy->migration_down_bytes;

            process_migrate_down_bytes(process, policy->migration_down_bytes);
            process_migrate_up_bytes(process, policy->migration_up_bytes);
        }
        tmp = process;
        process = process->next;
        pthread_mutex_unlock(&(tmp->process_lock));
    }
    /*if (dram_free_list.numentries != 0) {
      process = peek_process(&processes_list);
      while(process != NULL) {
        pthread_mutex_lock(&(process->process_lock));
        if(policy->dram_delta > 0) {
          process_migrate_up(process, policy->migration_up_bytes);
          policy->dram_delta -= policy->migration_up_bytes;
        }
        tmp = process;
        process = process->next;
        pthread_mutex_unlock(&(tmp->process_lock));
      }
    }*/
    /* } else if((FASTMEM_SIZE - memshare_proc) > delta_need) {
       process = peek_process(&processes_list);
       while(process != NULL) {
         pthread_mutex_lock(&(process->process_lock));
         if(policy->dram_delta > 0) {
           process_migrate_up(process, policy->migration_up_bytes);
           policy->dram_delta -= policy->migration_up_bytes;
         }
         tmp = process;
         process = process->next;
         pthread_mutex_unlock(&(tmp->process_lock));
       }
     } else {
       process = peek_process(&processes_list);
       while(process != NULL) {
         pthread_mutex_lock(&(process->process_lock));
         if(policy->dram_delta < 0) {
           process_migrate_down(process, policy->migration_down_bytes);
           policy->dram_delta *= -1;
           policy->dram_delta -= policy->migration_down_bytes;
           policy->dram_delta *= -1;
         }
         tmp = process;
         process = process->next;
         pthread_mutex_unlock(&(tmp->process_lock));
       }

       process = peek_process(&processes_list);
       while(process != NULL) {
         pthread_mutex_lock(&(process->process_lock));
         if(policy->dram_delta > 0) {
           process_migrate_up(process, policy->migration_up_bytes);
           policy->dram_delta -= policy->migration_up_bytes;
         }
         tmp = process;
         process = process->next;
         pthread_mutex_unlock(&(tmp->process_lock));
       }
     }*/
    //}
    // give share of migration bandwidth to processes that need more dram
    /*if(num_need_memory > 0) {
      delta_need = num_need_memory * FAIR_SHARE_DRAM;
      // Check if dram is available to provide
      if(((FASTMEM_SIZE - memshare_proc) > delta_need) && (down_migrations_left
    == 0)) { for(i = 0; i < num_need_memory; i++) { process =
    need_fastmem[i]; pthread_mutex_lock(&(process->process_lock));
          handle_ring_requests(process);

          // TODO : Can try giving everyone an equal share of migration
    bandwidth? if(policy->dram_delta == 0 && (up_migrations_left == 0)) {
            policy->dram_delta = (policy->ratio/memshare_need) *
    (interprocess_migrate); policy->migration_up_bytes =
    policy->dram_delta > FAIR_SHARE_DRAM ? FAIR_SHARE_DRAM : policy->dram_delta;
            policy->dram_delta = FAIR_SHARE_DRAM -
    policy->migration_up_bytes; process->still_migrating = true;
          }
          else {
            if(policy->dram_delta > ((policy->ratio/memshare_need) *
    (interprocess_migrate))) { policy->migration_up_bytes =
    ((policy->ratio/memshare_need) * (interprocess_migrate));
              policy->dram_delta -= policy->migration_up_bytes;
            }
            else {
              policy->migration_up_bytes = policy->dram_delta;
              policy->dram_delta -= policy->migration_up_bytes;
            }
          }

          if(policy->dram_delta == 0 && (process->still_migrating == true))
    { up_migrations_left += 1; process->still_migrating = false;
          }

          if(up_migrations_left == num_need_memory)
            up_migrations_left = 0;
          // now migrate ? Migrate = Give?
          process_migrate_up(process, policy->migration_up_bytes);

          pthread_mutex_unlock(&(process->process_lock));
        }
      } else { // free up dram first. TODO : what happens if we still dont
    have dram after free up?
        // migrate_down, then migrate_up ?
        // give share of migration bandwidth to processes that are giving up
    dram
        //process = peek_process(&processes_list);
        //while(process != NULL) {
        for(i = 0;i < num_take_memory; i++) {
          process = take_fastmem[i];
          pthread_mutex_lock(&(process->process_lock));

          // TODO : Can try giving everyone an equal share of migration
    bandwidth? if(policy->dram_delta == 0 && (down_migrations_left == 0)) {
            policy->dram_delta = (policy->ratio/(memshare_take)) *
    (interprocess_migrate); migrate_down_bytes = (policy->dram_delta) >
    (process->current_dram/2) ? (process->current_dram) :
    (policy->dram_delta); policy->dram_delta = ((process->current_dram /
    2) - migrate_down_bytes); process->still_migrating = true;
          }
          else {
            if((policy->dram_delta) > ((policy->ratio/(memshare_take)) *
    (interprocess_migrate))) { migrate_down_bytes =
    ((policy->ratio/(memshare_take)) * (interprocess_migrate));
              policy->dram_delta -= migrate_down_bytes;
            }
            else {
              migrate_down_bytes = policy->dram_delta;
              policy->dram_delta -= migrate_down_bytes;
            }
          }

          //migrate_down_bytes = migrate_down_bytes > interprocess_migrate ?
    interprocess_migrate : migrate_down_bytes; if(policy->dram_delta == 0
    && (process->still_migrating == true)) { down_migrations_left++;
            process->still_migrating = false;
          }

          policy->migration_down_bytes = migrate_down_bytes;
          // Do we keep migrating down until all (process->current_dram/2)
    is migrated? process_migrate_down(process,
    policy->migration_down_bytes);

          //tmp = process;
          //process = process->next;
          pthread_mutex_unlock(&(process->process_lock));
        }

        if(down_migrations_left == num_take_memory) {
          down_migrations_left = 0;
          for(i = 0; i < num_need_memory; i++) {
            process = need_fastmem[i];
            pthread_mutex_lock(&(process->process_lock));
            handle_ring_requests(process);

            // TODO : Can try giving everyone an equal share of migration
    bandwidth? if(policy->dram_delta == 0) { policy->dram_delta =
    (policy->ratio/memshare_need) * (interprocess_migrate);
              policy->migration_up_bytes = policy->dram_delta >
    FAIR_SHARE_DRAM ? FAIR_SHARE_DRAM : policy->dram_delta;
              policy->dram_delta = FAIR_SHARE_DRAM -
    policy->migration_up_bytes; process->still_migrating = true;
            }
            else {
              if(policy->dram_delta > ((policy->ratio/memshare_need) *
    (interprocess_migrate))) { policy->migration_up_bytes =
    ((policy->ratio/memshare_need) * (interprocess_migrate));
                policy->dram_delta -= policy->migration_up_bytes;
              }
              else {
                policy->migration_up_bytes = policy->dram_delta;
                policy->dram_delta -= policy->migration_up_bytes;
              }
            }

            if(policy->dram_delta == 0 && (process->still_migrating ==
    true)) { up_migrations_left += 1; process->still_migrating = false;
            }

            if(up_migrations_left == num_need_memory)
              up_migrations_left = 0;
            // now migrate ? Migrate = Give?
            process_migrate_up(process, policy->migration_up_bytes);

            pthread_mutex_unlock(&(process->process_lock));
          }
        }
      }
    }*/ /*else if(num_take_memory > 0) {
      for(i = 0;i<num_take_memory;i++) {
        process = take_fastmem[i];
        pthread_mutex_lock(&(process->process_lock));
        handle_ring_requests(process);

	// TODO : Can try giving everyone an equal share of migration bandwidth? 
	if(policy->dram_delta == 0) {
          policy->dram_delta = (-1) * (policy->ratio/memshare_take) * (interprocess_migrate);
          policy->migration_down_bytes = (-1 * policy->dram_delta) > (process->current_dram/2) ? (process->current_dram/2) : (-1 * policy->dram_delta);
          policy->dram_delta = (-1) * ((process->current_dram / 2) - policy->migration_down_bytes);
        }
        else {
          if((-1 * policy->dram_delta) > ((policy->ratio/memshare_take) * (interprocess_migrate))) {
            policy->migration_down_bytes = ((policy->ratio/memshare_take) * (interprocess_migrate)); 
            policy->dram_delta += policy->migration_down_bytes;
          }
          else {
            policy->migration_down_bytes = policy->dram_delta;
            policy->dram_delta += policy->migration_down_bytes;
          }
	}

        if(policy->dram_delta == 0) {
	  down_migrations_left++;
        }
        // Do we keep migrating down until all (process->current_dram/2) is migrated?
        process_migrate_down(process, migrate_down_bytes);
      }
    }*/
}

int fair_share_policy_init(policy_t *p, struct policy_opts *opts) {
    UNUSED(p);
    UNUSED(opts);
    // TODO
    assert(0);
}
