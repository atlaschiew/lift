#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#define FIXED_COLS 2 //wait in, floor
#define FIXED_ROWS 3 //border-top, border-bottom, boxtitle
#define FLOORS 26
#define LIFTS 3
#define LIFT_OUTER_WIDTH 7
#define LIFT_WIDTH 5
#define LIFT2LIFT_SPACE 3
#define LIFT_OC_INTERVAL 3 //OC = open close

void remove_element(int* , int, int);
void initialize(void);
void run(void);
void run_kill(void);
void get_default_lift_pos(int*);
void get_floor_name(int, char*);
void* run_lift(void *);
void question_clear(void);
void question_msg(char*);
void debug_str(char*);
void debug_int(int);
enum direction{DIRT_DOWN=0, DIRT_UP=1, DIRT_STOP=2,DIRT_STOP_UP=3,DIRT_STOP_DOWN=4}; 

struct lift {
	int id;
	char title[9];
	int pos;
	int default_pos;
	int board_id;
	pthread_t t;
	int direction;
	int wait_out;
	int wait_out_tickets[1000];
	int oc_count;
	int targets[FLOORS];
	
};

struct floor {
	int id;
	char title[10];
	int wait_in_up;
	int wait_in_down;
	int wait_in_up_tickets[1000];
	int wait_in_down_tickets[1000];
	int lift_job_up;
	int lift_job_down;
	
};

pthread_t t_draw;
struct lift lifts[LIFTS];
struct floor floors[FLOORS];

WINDOW *BOARD[LIFTS + 1];//0=wait in, board > 0 = lift

void remove_element(int *array, int index, int array_length) {
   int i;
   for(i = index; i < array_length; i++) array[i] = array[i + 1];
}

void run(void) {
	
	if(pthread_create(&t_draw, NULL, run_lift, NULL)!=0) {
		fprintf(stderr, "Error creating thread\n");
		exit(1);
		return;
	}
	
}

void run_kill(void) {
	int i;
	
	pthread_kill(t_draw, SIGUSR1);
	
    //erase every box and delete each window
    for (i = 0; i < FIXED_COLS + LIFTS; i++) {
        wborder(BOARD[i], ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
        wrefresh(BOARD[i]);
        delwin(BOARD[i]);
    }
	endwin();
}

void initialize(void) {
	
	int n = floor((FLOORS + 1)/LIFTS);
	int i;
	int start_board = 2;
	int lift_pos[LIFTS];
	
	initscr();     // initialize Ncurses
	noecho();      // disable echoing of characters on the screen
	curs_set( 0 ); // hide the default screen cursor.
	cbreak();
	
	clear();
	mvprintw(LINES - 1, (COLS - 5) / 2, "Simple Lift Program");
    refresh();
	
	get_default_lift_pos(lift_pos);

	for(i=0;i<LIFTS;i++) {
		lifts[i].id = i;
		lifts[i].wait_out = 0;
		lifts[i].pos = lift_pos[i];
		lifts[i].default_pos = lift_pos[i];
		lifts[i].board_id = start_board;
		lifts[i].direction = 2; //stop
		lifts[i].oc_count = -1;
		sprintf(lifts[i].title,"L%-4d", i+1);
		start_board++;
	}
	
	for(i=0;i<FLOORS;i++) {
		floors[i].id = i;
		floors[i].wait_in_up=0;
		floors[i].wait_in_down=0;
		floors[i].lift_job_up = -1;
		floors[i].lift_job_down = -1;
		char floor_name[LIFT_WIDTH];
		get_floor_name(i, floor_name);
		sprintf(floors[i].title, "%-3s", floor_name);
	}
	
	mvprintw(FIXED_ROWS + FLOORS , 0, "Enter to give new instruction...");
}

void main() {
	
	int key;
	int input_f_in=0,input_f_out=0;
	char confirm;
	initialize();
	
	if ((LINES < 24) || (COLS < 80)) {
        endwin();
        puts("Your terminal needs to be at least 80x24");
        exit(2);
    }
	
	run();

	do {
		noecho();
        key = getch();
		if(key > 0) {
			switch (key) {
				case 'q':
				case 'Q':
					
				break;
				
				case 10://KEY_ENTER
					
					echo();
					question_clear();
					mvprintw(FIXED_ROWS + FLOORS , 0, "Please enter which floor you are? ");
					refresh();
					scanw("%d", &input_f_in);
					
					if (input_f_in < 0 || input_f_in > FLOORS) {
						question_msg("Invalid from floor number (1)! Enter to give new instruction...");						
						break;
					}
					
					mvprintw(FIXED_ROWS + FLOORS + 1 , 0, "Please enter which floor you want to go? ");
					refresh();
					scanw("%d", &input_f_out);
					
					if (input_f_out < 0 || input_f_out > FLOORS) {
						question_msg("Invalid to floor number (2)! Enter to give new instruction...");						
						break;
					}
					
					if (input_f_in == input_f_out) {
						question_msg("Invalid floor number (3)! Enter to give new instruction...");						
						break;
					}
					
					mvprintw(FIXED_ROWS + FLOORS + 2 , 0, "Confirm? (y/n) ");
					refresh();
					scanw("%c", &confirm);
					
					if (confirm == 'y') {

						if (input_f_in > input_f_out) {
							
							int this_wait_in_down =  floors[input_f_in].wait_in_down;
							floors[input_f_in].wait_in_down_tickets[this_wait_in_down] = input_f_out;
							floors[input_f_in].wait_in_down++;
						} else {
							int this_wait_in_up = floors[input_f_in].wait_in_up;
							floors[input_f_in].wait_in_up_tickets[this_wait_in_up] = input_f_out;
							floors[input_f_in].wait_in_up++;
							
						}
						
						question_msg("Instruction stored successfully! Enter to give new instruction...");
						break;
					} else {
						question_msg("Submit fail! Enter to give new instruction...");
					}

				break;
				
			}
		}
    } while ((key != 'q') && (key != 'Q'));
	
	run_kill();
    exit(0);
}

void *run_lift(void* ptr)
{
    int i=0,j=0,k=0,l=0,m=0;
    int starty=0, startx=0;
	int board_id=0;

	//draw wait in slot
    BOARD[board_id] = newwin(FLOORS + FIXED_ROWS, LIFT_OUTER_WIDTH, starty,startx);
	wattron( BOARD[board_id], A_UNDERLINE );
	mvwprintw( BOARD[board_id], 1, 1, "%s", "Wait In" );
	wattroff( BOARD[board_id], A_UNDERLINE );
	
	//draw floor
	startx += (LIFT2LIFT_SPACE + LIFT_OUTER_WIDTH);
	board_id++;
	BOARD[board_id] = newwin(FLOORS + FIXED_ROWS, LIFT_OUTER_WIDTH, starty,startx);
	wattron( BOARD[board_id], A_UNDERLINE );
	mvwprintw( BOARD[board_id], 1, 1, "%s", "Floor" );
	wattroff( BOARD[board_id], A_UNDERLINE );

	//draw lift slot
    for (i = 0; i < (LIFTS); i++) {
		board_id = lifts[i].board_id;
        startx += (LIFT2LIFT_SPACE + LIFT_OUTER_WIDTH);
        BOARD[board_id] = newwin(FLOORS + FIXED_ROWS, LIFT_OUTER_WIDTH, starty,startx);
		wattron( BOARD[board_id], A_UNDERLINE );
		mvwprintw( BOARD[board_id], 1, 1, "%s", lifts[i].title);
		wattroff( BOARD[board_id], A_UNDERLINE );
    }

    // draw each floor
    for (i = 0; i < FIXED_COLS + LIFTS; i++) {
	    if (i == 0) {
			//draw wait in
			box(BOARD[i], ' ', ' ');
			
			for (j = 0; j < FLOORS; j++) {
				mvwprintw( BOARD[i], j+2, 2, "%d",0);
			}
		} else if (i == 1) {
			//draw floor
			box(BOARD[i], ' ', ' ');
			
			for (j = 0; j < FLOORS; j++) {
				int hfloor = (FLOORS - 1) - j;
				mvwprintw( BOARD[i], j+2, 2, "%s",floors[hfloor].title);
			}
			
		} else {
			//draw lift
			box(BOARD[i], '|', '-');
			
			int highlight = lifts[i- FIXED_COLS].pos;			
			
			for (j = 0; j < FLOORS; j++) {
				int hfloor = (FLOORS - 1) - j;
				char lift_content[LIFT_WIDTH];
				if (hfloor==highlight) {
					wattron( BOARD[i], A_STANDOUT );
					
					sprintf(lift_content,"%-3s", "0");
				} else {
					sprintf(lift_content,"%-3s", " ");
				}
				
				mvwprintw( BOARD[i], j+2, 2, "%s",lift_content);
				
				if (hfloor==highlight) {
					wattroff( BOARD[i], A_STANDOUT );
				}
			}
		}
		wrefresh(BOARD[i]);
    }
	
	sleep(1);//rest 1 second
	
	question_clear();
	i=0,j=0,k=0,l=0,m=0;
	int cfloor; //computer understand floor
	int hfloor;//human understand floor
	board_id = 0;
	
	while(1) {
		
		//update wait in display
		for (j = 0; j < FLOORS; j++) {
			hfloor = (FLOORS - 1) - j;
			mvwprintw( BOARD[0], j+2, 2, "%d",floors[hfloor].wait_in_up + floors[hfloor].wait_in_down);
		}
		
		wrefresh(BOARD[0]);
		
		//lift movement
		for(i=0;i<LIFTS;i++) {
			int cur_pos = lifts[i].pos;
			
			//process oc count down
			if (lifts[i].oc_count >= 0) {
				lifts[i].oc_count++;
	
				if (lifts[i].oc_count >= LIFT_OC_INTERVAL) {
					lifts[i].oc_count = -1;
					
					if(lifts[i].direction == DIRT_STOP_UP) {
						lifts[i].direction = DIRT_UP;
					} else if (lifts[i].direction == DIRT_STOP_DOWN) {
						lifts[i].direction = DIRT_DOWN;
					}
					
				} else {
					sleep(1);
					continue;
				}	
			}
			
			board_id = lifts[i].board_id;
			
			//remove previous highlight
			cfloor = (FLOORS - 1) - lifts[i].pos;
			wattroff( BOARD[board_id], A_STANDOUT );
			mvwprintw( BOARD[board_id], cfloor+2, 2, "%-3s"," ");
			
			//process current floor
			char lift_content[LIFT_WIDTH];
			
			//process wait out
			int total_wait_out = lifts[i].wait_out;
			for(k=0;k<total_wait_out;k++) {
				if (lifts[i].wait_out_tickets[k] == cur_pos) {
					lifts[i].oc_count = 0;
					remove_element(lifts[i].wait_out_tickets, k, total_wait_out);
					lifts[i].wait_out--;
					
				}
			}
			
			//process wait in
			int total_wait_in_up   = floors[cur_pos].wait_in_up;
			int total_wait_in_down = floors[cur_pos].wait_in_down;
			int total_wait_in = 0;
			
			int process_updown = 0;//1 = down, 2 = up
			int job_assign_you = (floors[cur_pos].lift_job_up == lifts[i].id || floors[cur_pos].lift_job_down == lifts[i].id);
			if (floors[cur_pos].lift_job_up == lifts[i].id) {
				process_updown = 2;
				total_wait_in = total_wait_in_up;
			
			} else if (floors[cur_pos].lift_job_down == lifts[i].id) {
				process_updown = 1;
				total_wait_in = total_wait_in_down;
			} else if (floors[cur_pos].lift_job_up == -1) {
				process_updown = 2;
				total_wait_in = total_wait_in_up;
			} else if (floors[cur_pos].lift_job_down == -1) {
				process_updown = 1;
				total_wait_in = total_wait_in_down;
			} else {
				process_updown = 0;
				total_wait_in = 0;
			}
			
			if (total_wait_in > 0 && process_updown > 0) {
				
				//move wait-in tickets to wait-out tickets
				int temp_wait_out = lifts[i].wait_out;
				
				for(k=0;k<total_wait_in;k++) {
					
					int go_floor;
					if (process_updown == 2) {
						go_floor = floors[cur_pos].wait_in_up_tickets[k];
					} else {
						go_floor = floors[cur_pos].wait_in_down_tickets[k];
					}
					
					if (go_floor >= 0) {
						int can_process = 0;
						
						if (!can_process) {
					
							if (job_assign_you) {
								can_process = 1;
								if (go_floor <= cur_pos) {
									lifts[i].direction = DIRT_DOWN;
								} else {
									lifts[i].direction = DIRT_UP;
								}
								
							}
						}
						
						if (!can_process) {
							can_process = ((lifts[i].direction == DIRT_UP) && go_floor > cur_pos) ? 1 : 0;
						}
						
						if (!can_process) {
							can_process = ((lifts[i].direction == DIRT_DOWN) && go_floor < cur_pos) ? 1 : 0;
						}
						
						if (!can_process) {
							if (cur_pos==0 && go_floor > cur_pos) {
								can_process = 1;
								lifts[i].direction = DIRT_UP;
							} 
						}
						
						if (!can_process) {
							
							if (cur_pos==(FLOORS - 1) && go_floor < cur_pos) {
								can_process = 1;
								lifts[i].direction = DIRT_DOWN;
							} 
						}
						
						if (can_process) {
							lifts[i].oc_count = 0;
							
							//register new lift's wait out ticket
							if (process_updown == 2) {
								
								lifts[i].wait_out_tickets[temp_wait_out] = floors[cur_pos].wait_in_up_tickets[k];
								floors[cur_pos].wait_in_up--;
								remove_element(floors[cur_pos].wait_in_up_tickets, k, total_wait_in);
								
								if (floors[cur_pos].wait_in_up == 0 && floors[cur_pos].lift_job_up==lifts[i].id) {
									floors[cur_pos].lift_job_up = -1;
								}
							} else {
								
								
								lifts[i].wait_out_tickets[temp_wait_out] = floors[cur_pos].wait_in_down_tickets[k];
								floors[cur_pos].wait_in_down--;
								remove_element(floors[cur_pos].wait_in_down_tickets, k, total_wait_in);
								
								if (floors[cur_pos].wait_in_down == 0 && floors[cur_pos].lift_job_down==lifts[i].id) {
									floors[cur_pos].lift_job_down = -1;
								}
							}
							
							temp_wait_out++;
							
						}
					}
				}
				
				//update lift's wait out figures
				lifts[i].wait_out = temp_wait_out;

			}
			
			//check is this lift free?
			int is_free_lift = 1;
			for (j = 0; j < FLOORS; j++) {
				if (floors[j].lift_job_up == lifts[i].id || floors[j].lift_job_down == lifts[i].id) {
					is_free_lift = 0;
					break;
				}
			}
			
			if (is_free_lift) {
				if (lifts[i].wait_out>0) {
					is_free_lift = 0;
				}
			}

			if (lifts[i].oc_count ==0) {
				//process new direction upon oc event
				if (is_free_lift) {
					//go back to default pos
					if (lifts[i].pos > lifts[i].default_pos) {
						lifts[i].direction = DIRT_STOP_DOWN;
					} else {
						lifts[i].direction = DIRT_STOP_UP;
					
					}
				} else {
					lifts[i].direction = (lifts[i].direction == DIRT_DOWN) ? DIRT_STOP_DOWN : DIRT_STOP_UP;
				}
				
				lifts[i].oc_count++;
			} else {
				//go back to default pos
				if (is_free_lift) {
					
					if (lifts[i].pos > lifts[i].default_pos) {
						lifts[i].direction = DIRT_DOWN;
					} else if (lifts[i].pos < lifts[i].default_pos) {
						lifts[i].direction = DIRT_UP;
					
					}
				}
			}
			
			//if lift free, keep stop in default pos
			if (is_free_lift) {
				if (lifts[i].pos == lifts[i].default_pos) {
					lifts[i].direction = DIRT_STOP;
				} 
			}
			
			//new pos to go based on current direction
			if (lifts[i].direction == DIRT_UP) {
				lifts[i].pos++;
			} else if (lifts[i].direction == DIRT_DOWN) {
				lifts[i].pos--;
			}
			
			//update new pos
			sprintf(lift_content,"%-3d", lifts[i].wait_out);
			cfloor = (FLOORS - 1) - lifts[i].pos;
			wattron( BOARD[board_id], A_STANDOUT );
			mvwprintw( BOARD[board_id], cfloor +2, 2, "%-3s",lift_content);
			wattroff( BOARD[board_id], A_STANDOUT );
			wrefresh(BOARD[board_id]);
		}
		
		//===========================
		//System coordinate lift job
		//===========================
		
		//check any free lifts;
		int free_lift=0;
		int free_lifts[LIFTS];
		
		for(j=0; j <LIFTS; j++) {

			if (lifts[j].wait_out==0) {
				free_lifts[free_lift] = j;
				free_lift++;
			}
		}
		
		for (j = 0; j < FLOORS; j++) {
			
			if (!free_lift) {
				break;
			}
			
			if (floors[j].lift_job_up > -1) {
				for(k=0; k <free_lift;k++ ) {
					if (free_lifts[k] ==floors[j].lift_job_up) {
						remove_element(free_lifts, k, free_lift);
						free_lift--;
						break;
					}
				}
			}
			
			if (floors[j].lift_job_down > -1) {
				for(k=0; k <free_lift;k++ ) {
					if (free_lifts[k] ==floors[j].lift_job_down) {
						remove_element(free_lifts, k, free_lift);
						free_lift--;
						break;
					}
				}
			}
		}
		
		for (j = 0; j < FLOORS; j++) {
			
			if (!free_lift) {
				break;
			}

			int total_wait_in_up   = floors[j].wait_in_up;
			int total_wait_in_down = floors[j].wait_in_down;
			int total_wait_in = 0;
			
			if (total_wait_in_up+total_wait_in_down <= 0)
				continue;
			else if (floors[j].lift_job_up >= 0 && floors[j].lift_job_down >=0)
				continue;

			int pending_task = 0;
			int pending_tasks[2];
			
			if (floors[j].lift_job_down == -1 && total_wait_in_down > 0) {
				pending_tasks[pending_task] = 1;
				pending_task++;
			}
			
			if (floors[j].lift_job_up == -1 && total_wait_in_up > 0) {
				pending_tasks[pending_task] = 2;
				pending_task++;
			}
			
			if (!pending_task) {
				continue;
			} else {
				
				for(k=0; k < pending_task;k++) {

					if (!free_lift) {
						break;
					}
					
					int shortest_distance =FLOORS;
					int selected_lift = -1;
					int free_lift_idx = -1;
					int this_distance;
					for(l=0; l < free_lift; l++) {
						
						int lift_id = lifts[free_lifts[l]].id;
						
						if (lifts[lift_id].direction == DIRT_STOP) {
							
							this_distance = lifts[lift_id].pos - j;
							
							if (abs(this_distance) < abs(shortest_distance)) {
								shortest_distance = this_distance;
								selected_lift = lift_id;
								free_lift_idx = l;
							}
						}
					}
			
					if(selected_lift >= 0) {
						
						if (pending_tasks[k] == 2 && floors[j].lift_job_up == -1) {
							floors[j].lift_job_up = selected_lift;
							
						} else if (pending_tasks[k] == 1 && floors[j].lift_job_down == -1) {
							floors[j].lift_job_down = selected_lift;
						}
						
						if (shortest_distance<= 0) {
							lifts[selected_lift].direction = DIRT_UP;
							
							
						} else {
							lifts[selected_lift].direction = DIRT_DOWN;
							
						}
						
						remove_element(free_lifts, free_lift_idx, free_lift);
						free_lift--;
						
					}
				}
			}
		}
		
		sleep(1);
	}
}

void get_default_lift_pos(int* ret) {
	int n = floor((FLOORS + 1)/LIFTS);
	int i;
	for(int i=0;i<LIFTS;i++) {
		*(ret+i) = i * n;
	}
}

void get_floor_name(int floor, char* floor_name) {
	
	char fi[7] = "";
	
	strcat(fi, "%-");
	strcat(fi, "3");
	
	if (floor == 0) {
		strcat(fi, "s");
		sprintf(floor_name, fi, "G");
	} else {
		strcat(fi, "d");
		sprintf(floor_name, fi, floor);
	}
}

void question_msg(char* msg) {
	question_clear();
	
	mvprintw(FIXED_ROWS + FLOORS , 0, msg);
	refresh();
}
	
void question_clear(void) {
	
	move(FIXED_ROWS + FLOORS, 0); 
	clrtoeol();//clear line
	
	move(FIXED_ROWS + FLOORS + 1, 0); 
	clrtoeol();//clear line
	
	move(FIXED_ROWS + FLOORS + 2, 0); 
	clrtoeol();//clear line
	
	mvprintw(FIXED_ROWS + FLOORS , 0, "Enter to give new instruction...");
	
	refresh();
	
}

void debug_int(int v) {
	
	char str[10];
	sprintf(str, "---%d---", v);
	debug_str(str);	
}

void debug_str(char* str) {
	
	mvprintw(LINES - 1, (COLS - 5) / 2, str);
    refresh();
}
