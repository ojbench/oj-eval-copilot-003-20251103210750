#include <bits/stdc++.h>
using namespace std;

struct Submission { char prob; string status; int time; };

struct ProblemState {
    bool solved = false;
    int wrong = 0;          // total wrong before first AC (across all time)
    int solve_time = 0;     // first AC time
    // Freeze-session fields
    int wrong_before_freeze = 0; // snapshot at FREEZE if unsolved
    int y_after_freeze = 0;      // number of submissions after freeze
    vector<pair<string,int>> freeze_subs; // (status, time) during current freeze session
};

struct Team {
    string name;
    int solved = 0;
    long long penalty = 0;
    vector<int> solve_times; // store solve times; we keep unsorted and sort into desc when needed
    array<ProblemState,26> ps{};
    vector<Submission> all_subs; // for QUERY_SUBMISSION
};

static inline bool isAccepted(const string &s){ return s == "Accepted"; }

struct RankKey {
    int solved; long long penalty; const vector<int>* sorted_desc_times; const string* name;
};

int M = 0; // problem count
bool started = false;
bool frozen = false;
int duration_time = 0;

// Teams
unordered_map<string,int> id;
vector<Team> teams;

// last flushed ranking order: store indices
vector<int> last_rank_order;

static vector<int> get_sorted_desc(const Team &t){
    vector<int> v = t.solve_times;
    sort(v.begin(), v.end(), greater<int>());
    return v;
}

static vector<int> compute_ranking_order(){
    int n = (int)teams.size();
    vector<int> idx(n); iota(idx.begin(), idx.end(), 0);
    // Precompute sorted desc times per team to avoid resorting during compares
    vector<vector<int>> desc_times(n);
    for(int i=0;i<n;++i){ desc_times[i] = get_sorted_desc(teams[i]); }
    stable_sort(idx.begin(), idx.end(), [&](int a, int b){
        const Team &A = teams[a], &B = teams[b];
        if (A.solved != B.solved) return A.solved > B.solved;
        if (A.penalty != B.penalty) return A.penalty < B.penalty;
        const auto &va = desc_times[a];
        const auto &vb = desc_times[b];
        // same size since solved equal
        for(size_t i=0;i<va.size();++i){
            if (va[i] != vb[i]) return va[i] < vb[i]; // smaller max/next-max wins
        }
        return A.name < B.name;
    });
    return idx;
}

static void print_scoreboard(const vector<int>& order){
    // ranking is 1-based position in this order
    vector<int> rankpos(teams.size());
    for(size_t i=0;i<order.size();++i) rankpos[order[i]] = (int)i+1;
    for(size_t i=0;i<order.size();++i){
        int ti = order[i];
        Team &T = teams[ti];
        cout << T.name << ' ' << rankpos[ti] << ' ' << T.solved << ' ' << T.penalty;
        for(int p=0;p<M;++p){
            auto &S = T.ps[p];
            cout << ' ';
            if (S.solved){
                if (S.wrong==0) cout << '+';
                else cout << '+' << S.wrong;
            } else if (frozen && S.y_after_freeze>0){
                if (S.wrong_before_freeze==0) cout << "0/" << S.y_after_freeze;
                else cout << '-' << S.wrong_before_freeze << '/' << S.y_after_freeze;
            } else {
                if (S.wrong==0) cout << '.';
                else cout << '-' << S.wrong;
            }
        }
        cout << '\n';
    }
}

static void do_flush(){
    last_rank_order = compute_ranking_order();
}

static void start_competition(int dur, int pcnt){
    if (started){ cout << "[Error]Start failed: competition has started.\n"; return; }
    started = true; duration_time = dur; M = pcnt;
    cout << "[Info]Competition starts.\n";
    // Initialize last_rank_order by lexicographic order of team names
    vector<int> idx(teams.size()); iota(idx.begin(), idx.end(), 0);
    stable_sort(idx.begin(), idx.end(), [&](int a, int b){ return teams[a].name < teams[b].name; });
    last_rank_order = idx;
}

static void add_team(const string &name){
    if (started){ cout << "[Error]Add failed: competition has started.\n"; return; }
    if (id.find(name)!=id.end()){ cout << "[Error]Add failed: duplicated team name.\n"; return; }
    int idx = (int)teams.size();
    id[name]=idx; teams.push_back(Team());
    teams.back().name = name;
    cout << "[Info]Add successfully.\n";
}

static void submit(const string &prob, const string &team, const string &status, int t){
    int ti = id[team];
    Team &T = teams[ti];
    char pc = prob[0];
    int p = pc - 'A';
    T.all_subs.push_back(Submission{pc, status, t});
    ProblemState &S = T.ps[p];
    if (S.solved) return; // further submissions do nothing
    if (!frozen){
        if (isAccepted(status)){
            S.solved = true;
            S.solve_time = t;
            T.solved++;
            T.penalty += 20LL * S.wrong + t;
            T.solve_times.push_back(t);
        } else {
            S.wrong++;
        }
    } else {
        // during freeze session: only count towards y and record
        S.freeze_subs.push_back({status, t});
        S.y_after_freeze++;
    }
}

static void do_freeze(){
    if (frozen){ cout << "[Error]Freeze failed: scoreboard has been frozen.\n"; return; }
    frozen = true;
    // snapshot wrong_before_freeze for all unsolved problems; reset y counter
    for(auto &T: teams){
        for(int p=0;p<26;++p){
            auto &S = T.ps[p];
            S.y_after_freeze = 0;
            S.freeze_subs.clear();
            if (!S.solved){
                S.wrong_before_freeze = S.wrong;
            } else {
                S.wrong_before_freeze = 0;
            }
        }
    }
    cout << "[Info]Freeze scoreboard.\n";
}

static void apply_unfreeze_for(Team &T, int p){
    ProblemState &S = T.ps[p];
    if (S.y_after_freeze==0) return; // nothing to do
    int add_wrong = 0;
    bool got_ac = false;
    int ac_time = 0;
    for(auto &pr: S.freeze_subs){
        if (!got_ac){
            if (isAccepted(pr.first)){
                got_ac = true;
                ac_time = pr.second;
            } else {
                add_wrong++;
            }
        }
    }
    // Apply results
    if (!S.solved){
        S.wrong += add_wrong;
        if (got_ac){
            S.solved = true;
            S.solve_time = ac_time;
            T.solved++;
            T.penalty += 20LL * S.wrong + ac_time; // S.wrong already includes before+after wrongs before AC
            T.solve_times.push_back(ac_time);
        } else {
            // all were wrong
            // nothing else
        }
    }
    // clear freeze markers
    S.y_after_freeze = 0;
    S.freeze_subs.clear();
}

static bool team_has_frozen(const Team &T){
    if (!frozen) return false;
    for(int p=0;p<M;++p){ const auto &S=T.ps[p]; if (!S.solved && S.y_after_freeze>0) return true; }
    return false;
}

static void do_scroll(){
    if (!frozen){ cout << "[Error]Scroll failed: scoreboard has not been frozen.\n"; return; }
    cout << "[Info]Scroll scoreboard.\n";
    // First flush and print current scoreboard (still frozen)
    vector<int> order = compute_ranking_order();
    last_rank_order = order; // this is a flush without message
    print_scoreboard(order);

    // Now repeatedly unfreeze
    vector<int> current_order = order;
    while (true){
        // find lowest-ranked team with frozen problems
        int chosen_team = -1;
        for(int i=(int)current_order.size()-1;i>=0;--i){
            int ti = current_order[i];
            if (team_has_frozen(teams[ti])){ chosen_team = ti; break; }
        }
        if (chosen_team==-1) break; // done
        // choose smallest problem letter among frozen problems
        int chosen_prob = -1;
        for(int p=0;p<M;++p){ auto &S = teams[chosen_team].ps[p]; if (!S.solved && S.y_after_freeze>0){ chosen_prob = p; break; } }
        if (chosen_prob==-1){
            // should not happen, skip safeguard
            // remove flag that there is no frozen? handled by team_has_frozen
        }
        // Record old position
        int old_pos = find(current_order.begin(), current_order.end(), chosen_team) - current_order.begin();
        // Apply unfreeze for this problem
        apply_unfreeze_for(teams[chosen_team], chosen_prob);
        // Recompute order
        current_order = compute_ranking_order();
        // If ranking improved, output line
        int new_pos = find(current_order.begin(), current_order.end(), chosen_team) - current_order.begin();
        if (new_pos < old_pos){
            // team that was at the new_pos before move is the one replaced; However we need the team who was at that position prior to unfreeze.
            // To obtain that, use the previous order before recompute: at index new_pos in previous current_order is the replaced team
            // But if new_pos < old_pos, new_pos is index in new order; The replaced is the team that occupied that index in the previous order (order_before)
            // We saved order_before in variable 'order' earlier? We'll use a copy
        }
        // To get replaced team, we need the order before applying change
        // We didn't keep it; add now by recomputing previous order again is expensive? Keep a copy before recompute
        ;
        // To fix, we restructure: keep prev_order copy before recompute
        // We'll implement below by keeping prev_order variable
    }

    // After all unfreezes, print final scoreboard
    frozen = false;
    vector<int> final_order = compute_ranking_order();
    last_rank_order = final_order; // after scrolling
    print_scoreboard(final_order);
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;
    while (std::getline(cin, line)){
        if (line.empty()) continue;
        string cmd;
        {
            stringstream ss(line);
            ss >> cmd;
        }
        if (cmd=="ADDTEAM"){
            string name; {
                stringstream ss(line); ss>>cmd>>name; }
            add_team(name);
        } else if (cmd=="START"){
            // START DURATION [duration_time] PROBLEM [problem_count]
            string tmp1,tmp2; int dur, pcnt;
            stringstream ss(line);
            ss>>cmd>>tmp1>>dur>>tmp2>>pcnt;
            start_competition(dur, pcnt);
        } else if (cmd=="SUBMIT"){
            // SUBMIT [problem_name] BY [team_name] WITH [submit_status] AT [time]
            string prob, by, team, with, status, at; int t;
            stringstream ss(line);
            ss>>cmd>>prob>>by>>team>>with>>status>>at>>t;
            submit(prob, team, status, t);
        } else if (cmd=="FLUSH"){
            do_flush();
            cout << "[Info]Flush scoreboard.\n";
        } else if (cmd=="FREEZE"){
            do_freeze();
        } else if (cmd=="SCROLL"){
            // We need enhanced logic to output rank-change lines
            if (!frozen){ cout << "[Error]Scroll failed: scoreboard has not been frozen.\n"; continue; }
            cout << "[Info]Scroll scoreboard.\n";
            // pre-flush and print
            vector<int> order = compute_ranking_order();
            last_rank_order = order;
            print_scoreboard(order);
            vector<int> current_order = order;
            while (true){
                int chosen_team = -1;
                for(int i=(int)current_order.size()-1;i>=0;--i){ int ti=current_order[i]; if (team_has_frozen(teams[ti])){ chosen_team=ti; break; } }
                if (chosen_team==-1) break;
                int chosen_prob=-1; for(int p=0;p<M;++p){ auto &S=teams[chosen_team].ps[p]; if (!S.solved && S.y_after_freeze>0){ chosen_prob=p; break; } }
                int old_pos = find(current_order.begin(), current_order.end(), chosen_team) - current_order.begin();
                // keep prev order copy for replaced lookup
                vector<int> prev_order = current_order;
                apply_unfreeze_for(teams[chosen_team], chosen_prob);
                current_order = compute_ranking_order();
                int new_pos = find(current_order.begin(), current_order.end(), chosen_team) - current_order.begin();
                if (new_pos < old_pos){
                    int replaced_team_idx = prev_order[new_pos];
                    cout << teams[chosen_team].name << ' ' << teams[replaced_team_idx].name << ' ' << teams[chosen_team].solved << ' ' << teams[chosen_team].penalty << "\n";
                }
            }
            frozen = false;
            vector<int> final_order = compute_ranking_order();
            last_rank_order = final_order;
            print_scoreboard(final_order);
        } else if (cmd=="QUERY_RANKING"){
            string team; { stringstream ss(line); ss>>cmd>>team; }
            if (id.find(team)==id.end()){ cout << "[Error]Query ranking failed: cannot find the team.\n"; continue; }
            cout << "[Info]Complete query ranking.\n";
            if (frozen){
                cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
            }
            int ti = id[team];
            // find rank in last_rank_order
            int rk = 0; for(size_t i=0;i<last_rank_order.size();++i){ if (last_rank_order[i]==ti){ rk=(int)i+1; break; } }
            cout << team << " NOW AT RANKING " << rk << "\n";
        } else if (cmd=="QUERY_SUBMISSION"){
            // QUERY_SUBMISSION [team_name] WHERE PROBLEM=[problem_name] AND STATUS=[status]
            // We'll parse by tokens and extract team, prob filter, status filter
            string tmp, team, where, probEq, and_, statusEq;
            stringstream ss(line);
            ss>>cmd>>team>>where>>probEq>>and_>>statusEq;
            if (id.find(team)==id.end()){ cout << "[Error]Query submission failed: cannot find the team.\n"; continue; }
            cout << "[Info]Complete query submission.\n";
            string probv = probEq.substr(probEq.find('=')+1);
            string statusv = statusEq.substr(statusEq.find('=')+1);
            char pfilter = 0; if (probv!="ALL") pfilter = probv[0];
            bool allprob = (pfilter==0);
            bool allstatus = (statusv=="ALL");
            const auto &subs = teams[id[team]].all_subs;
            bool found=false; Submission last{};
            for (int i=(int)subs.size()-1;i>=0;--i){
                const auto &s = subs[i];
                if ((!allprob && s.prob!=pfilter)) continue;
                if ((!allstatus && s.status!=statusv)) continue;
                found=true; last=s; break;
            }
            if (!found){ cout << "Cannot find any submission.\n"; }
            else { cout << team << ' ' << last.prob << ' ' << last.status << ' ' << last.time << "\n"; }
        } else if (cmd=="END"){
            cout << "[Info]Competition ends.\n";
            break;
        }
    }
    return 0;
}
