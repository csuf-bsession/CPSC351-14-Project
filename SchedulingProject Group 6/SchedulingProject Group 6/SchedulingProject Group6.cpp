#include <iostream>
#include <unordered_map>
#include <map>
#include <string>
#include <algorithm>
#include <queue>

using namespace std;

//Metrics Storage using unordered maps
struct Result
{
	map<int, int> completiontime; // pid, completion timestamp
	map<int, int> turnaroundtime; // pid, completion time - arrival time: map<int,Time> // pid (key), arrival time
	map<int, int> waitingtime; // pid, first start - arrival
	map<int, int> firststarttime; //first time cooking began for the whole simulation to use for calculating waiting time.
	double avg_turnaround;
	double avg_waiting; //initialize struct values to 0 so we don't get garbage numbers

	Result()
	{
		completiontime = { { 0, 0 } };
		turnaroundtime = { { 0, 0 } };
		waitingtime = { { 0, 0 } };
		firststarttime = { { 0, 0 } };
		avg_turnaround = 0;
		avg_waiting = 0;
	}
};

struct object
{
	string name;
	string type;
	int priorityvalue;
	int bursttime;

	object()
	{
		name = "";
		type = "";
		priorityvalue = 0;
		bursttime = 0;
	}
};

struct table
{
	vector<object> orderlist;
	int tableid;
	int arrivaltime;
	int groupsize;

	table()
	{
		orderlist = {};
		tableid = 0;
		arrivaltime = 0;
		groupsize = 0;
	}

	//below acts in a way where a table is complete after all of the products are done, 
	//giving us the sum of prep time.
	int get_cooking_time() const
	{
		int totaltime = 0;
		for (int i = 0; i < orderlist.size(); i++) {
			totaltime += orderlist[i].bursttime;
		}
		return totaltime;
	}

	bool has_apps_drinks()
	{
		for (int i = 0; i < orderlist.size(); i++) {
			if (orderlist[i].type == "Appetizer" || orderlist[i].type == "Drink") {
				return true;
				break;
			}
		}
		return false;
	}

	int compute_priority() {
		int score = 0; //higher num=higher prio
		if (has_apps_drinks() == true) { score += 3; }
		if (get_cooking_time() <= 5) { score += 2; }
		if (groupsize >= 4) { score += 1; }
		return score;
	}
};

//I’m making another struct to contain a “slice” of continuous cooking session for one table 
//so we don’t have to keep track of three separate arrays for each algorithm we have. 
//Also makes printing easy. Acts like a receipt.
struct ScheduleSlice {
	int pid; //table id
	int starttime; //when this table started cooking
	int endtime; //when table is finished cooking, = to start    table.get_cooking_time()
};

//This is a linear scan which is fine for the scope of our project. 
//If we were doing hundreds of orders then this would not be it. 
//It scans through the table vector and returns a pointer to the table whose pid matches.

const table* find_by_pid(const vector<table>& tables, int pid) {
	for (const auto& t : tables)
	{
		if (t.tableid == pid)
		{
			return &t;
		} //returns ptr to found table
	}
	return nullptr;
}

//to replace the while (time < 99999) loop for better efficiency and timeliness 
//we will make a function to jump time to the next arrival/real event, so we don’t 
//wait until a table is ready, slowly incrementing time by 1.
//NOTE: assumes we have already sorted by arrival time, 
//which we will have done in the algos prior to using this.
int jump_to_next_arrival_if_needed(int timenow, const vector<table>& sorted_by_arrival, size_t i)
{
	if (i < sorted_by_arrival.size())
	{
		return max(timenow, sorted_by_arrival[i].arrivaltime);
	}

	else
	{
		return timenow; // means no moreall arrivals
	}
}

//Compute metrics function like completion time, turnaround, wait
//the ampersand symbol & creates a reference so we don't make a full copy of each table! Saves memory.
//”schedule” is our timeline using the list of receipts via //ScheduleSlice
//this is for per table metrics
Result compute_metrics(const vector<table>& tables, const vector<ScheduleSlice>& schedule) {
	Result R;
	//this section is to fill completion time and record first start time using our “receipt”, and with non-preemptive each pid appears only once
	for (const auto& s : schedule) {
		R.completiontime[s.pid] = s.endtime;
		if (!R.firststarttime.count(s.pid))
		{
			R.firststarttime[s.pid] = s.starttime; // if its the first time seeing this pid, record its first_start
		}
	}

	//now we compute for each table. TAT = completion - arrival and //Wait = first_start - arrival
	for (const auto& t : tables) {
		int pid = t.tableid;
		int arrival = t.arrivaltime;
		int completion = R.completiontime[t.tableid];
		int firststart = R.firststarttime[t.tableid];
		R.turnaroundtime[t.tableid] = completion - arrival;
		R.waitingtime[t.tableid] = firststart - arrival;
	}
	//We first find the sums of each time type: (TAT or Wait)
	//to clarify, the value kv has type ‘pair<const int, Time>
	//which means kv.first is the pid or the const int, and //kv.second is the value we want for the sum.
	double sum_tat = 0.0;
	for (auto& kv : R.turnaroundtime)
	{
		sum_tat += kv.second;
	}

	double sum_wait = 0.0;
	for (auto& kv : R.waitingtime)
	{
		sum_wait += kv.second;
	}
	//now we finally calculate the average after getting the sums
	int n = static_cast<int>(tables.size());
	if (n > 0) {
		R.avg_turnaround = sum_tat / n;
		R.avg_waiting = sum_wait / n;
	}
	return R;
}


vector<table> stored_maps = {}; // would be stored in the main function most likely, represents list of sorted maps







// run the fcfs algorithm on tables
Result run_fcfs(vector<table> tables)
{
	// we say that a table is served/complete when all the products are finished cooking in order to not overcomplicate things.
		// sort it first, because otherwise it could break the code
	sort(tables.begin(), tables.end(), [](const table& a, const table& b)
		{
			//earlier arrival comes first using this if statement
			if (a.arrivaltime != b.arrivaltime)
			{
				return a.arrivaltime < b.arrivaltime;
			}
			return a.tableid < b.tableid; //our tiebreaker using pid’s 
		});

	queue<int> ready; //FIFO so we use queues. Labeled as the ready queue
	vector<ScheduleSlice> schedule; //List of “receipts”
	int timenow = 0; //simulation clock
	size_t i = 0; //next arrival index
	int completed = 0;
	const int N = static_cast<int>(tables.size()); // table size, forced to integer type just to make sure.



	//step 1 Feeding newly arrived table orders into our ready queue
	//i points to next table in sorted list
	//logic goes: “while there are tables that have arrived
	//already or are arriving NOW, add them to the rdy queue.
	//Like the waiter handing the order ticket to the kitchen.
	while (completed < N)
	{
		while (i < tables.size() && tables[i].arrivaltime <= timenow)
		{
			ready.push(tables[i].tableid);
			cout << "Scanned table (tableid: " << tables[i].tableid << ")" << endl;
			++i; //move to next unprocessed table
		}

		//step 2 now we handle the idle kitchen periods, and this also increments our “now” clock value while skipping the tedious 1 by 1 incrementation that the prior Time < 99999 loop would force us to do.
		if (ready.empty())
		{
			timenow = jump_to_next_arrival_if_needed(timenow, tables, i);
			continue; //restart loop after jump
		}

		//Step 3 start the FIFO
		int pid = ready.front(); //oldest waiting table to front of queue (FIFO)
		ready.pop(); //it is now being cooked. Removed from queue

		const table* t = find_by_pid(tables, pid); //pass list of all tables and current pid and store the pointer to matching table in t. We can also make a cerr output if pid is not found.


		//start = max(now, arrival) in case kitchen was idle
		int timestart = max(timenow, t->arrivaltime);
		int timeend = timestart + t->get_cooking_time();
		//record cooking interval
		schedule.push_back({ pid, timestart, timeend });
		timenow = timeend; //Time moves forward to when job is finished
		++completed;
	}
	return compute_metrics(tables, schedule);
}




int main()
{
	table table1;
	table table2;
	table table3;
	object toast;
	object veggies;
	object soda;

	toast.name = "Toast";
	toast.type = "Appetizer";
	toast.priorityvalue = 0;
	toast.bursttime = 3;

	veggies.name = "Veggies";
	veggies.type = "Main Course";
	veggies.priorityvalue = 0;
	veggies.bursttime = 5;

	soda.name = "Soda";
	soda.type = "Drink";
	soda.priorityvalue = 0;
	soda.bursttime = 1;

	table1.orderlist = { toast, toast, veggies };
	table1.tableid = 1;
	table1.arrivaltime = 3;
	table1.groupsize = 1;

	table2.orderlist = { veggies, soda };
	table2.tableid = 2;
	table2.arrivaltime = 2;
	table2.groupsize = 2;

	table3.orderlist = { toast,toast,toast,toast };
	table3.tableid = 3;
	table3.arrivaltime = 1;
	table3.groupsize = 3;

	stored_maps.push_back(table1);
	stored_maps.push_back(table2);
	stored_maps.push_back(table3);

	Result resultings = run_fcfs(stored_maps);
	cout << endl << resultings.avg_turnaround << endl << endl << endl;



	return 0;
}