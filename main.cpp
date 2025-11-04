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

static void insert_desc(vector<int>& v, int t){
    auto it = lower_bound(v.begin(), v.end(), t, greater<int>());
    v.insert(it, t);
}

static vector<int> compute_ranking_order(){
    int n = (int)teams.size();
    vector<int> idx(n); iota(idx.begin(), idx.end(), 0);
    stable_sort(idx.begin(), idx.end(), [&](int a, int b){
        const Team &A = teams[a], &B = teams[b];
        if (A.solved != B.solved) return A.solved > B.solved;
        if (A.penalty != B.penalty) return A.penalty < B.penalty;
        const auto &va = A.solve_times; // maintained in descending order
        const auto &vb = B.solve_times;
        for (size_t i=0;i<va.size(); ++i){
            if (va[i] != vb[i]) return va[i] < vb[i]; // smaller max/next-max wins
        }
        return A.name < B.name;
    });
    return idx;
}

static void print_scoreboard(const vector<int>& order){
    for(size_t i=0;i<order.size();++i){
        int ti = order[i];
        Team &T = teams[ti];
        cout << T.name << ' ' << (i+1) << ' ' << T.solved << ' ' << T.penalty;
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
            insert_desc(T.solve_times, t);
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
            insert_desc(T.solve_times, ac_time);
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

    string cmd;
    while (cin >> cmd){
        if (cmd=="ADDTEAM"){
            string name; cin >> name; add_team(name);
        } else if (cmd=="START"){
            string DURATION, PROBLEM; int dur, pcnt; cin >> DURATION >> dur >> PROBLEM >> pcnt; start_competition(dur, pcnt);
        } else if (cmd=="SUBMIT"){
            string prob, BY, team, WITH, status, AT; int t; cin >> prob >> BY >> team >> WITH >> status >> AT >> t; submit(prob, team, status, t);
        } else if (cmd=="FLUSH"){
            do_flush(); cout << "[Info]Flush scoreboard.\n";
        } else if (cmd=="FREEZE"){
            do_freeze();
        } else if (cmd=="SCROLL"){
            if (!frozen){ cout << "[Error]Scroll failed: scoreboard has not been frozen.\n"; continue; }
            cout << "[Info]Scroll scoreboard.\n";
            vector<int> current_order = compute_ranking_order();
            last_rank_order = current_order;
            print_scoreboard(current_order);
            int n = (int)teams.size();
            vector<int> pos(n);
            for (int i=0;i<n;++i) pos[current_order[i]] = i;
            auto has_frozen_team = [&](int ti){ return team_has_frozen(teams[ti]); };
            struct CmpByPos { bool operator()(const pair<int,int>& a, const pair<int,int>& b) const { if (a.first!=b.first) return a.first<b.first; return a.second<b.second; } };
            set<pair<int,int>, CmpByPos> cand;
            vector<char> in_set(n, 0);
            for (int ti=0; ti<n; ++ti){ if (has_frozen_team(ti)){ cand.insert({pos[ti], ti}); in_set[ti]=1; } }
            auto better_than = [&](int a, int b){
                const Team &A = teams[a], &B = teams[b];
                if (A.solved != B.solved) return A.solved > B.solved;
                if (A.penalty != B.penalty) return A.penalty < B.penalty;
                const auto &va = A.solve_times;
                const auto &vb = B.solve_times;
                for (size_t i=0;i<va.size(); ++i){ if (va[i]!=vb[i]) return va[i] < vb[i]; }
                return A.name < B.name;
            };
            while (!cand.empty()){
                auto it = prev(cand.end());
                int chosen_team = it->second;
                cand.erase(it); in_set[chosen_team]=0;
                int chosen_prob=-1; for(int p=0;p<M;++p){ auto &S=teams[chosen_team].ps[p]; if (!S.solved && S.y_after_freeze>0){ chosen_prob=p; break; } }
                int old_pos = pos[chosen_team];
                vector<int> prev_order = current_order;
                apply_unfreeze_for(teams[chosen_team], chosen_prob);
                int i = pos[chosen_team];
                while (i>0 && better_than(chosen_team, current_order[i-1])){
                    int other = current_order[i-1];
                    swap(current_order[i], current_order[i-1]);
                    int old_pos_other = pos[other];
                    pos[other] = i; pos[chosen_team] = i-1;
                    if (in_set[other]){
                        cand.erase({old_pos_other, other});
                        cand.insert({pos[other], other});
                    }
                    --i;
                }
                int new_pos = pos[chosen_team];
                if (new_pos < old_pos){
                    int replaced_team_idx = prev_order[new_pos];
                    cout << teams[chosen_team].name << ' ' << teams[replaced_team_idx].name << ' ' << teams[chosen_team].solved << ' ' << teams[chosen_team].penalty << "\n";
                }
                if (has_frozen_team(chosen_team)){
                    cand.insert({pos[chosen_team], chosen_team});
                    in_set[chosen_team]=1;
                }
            }
            frozen = false;
            last_rank_order = current_order;
            print_scoreboard(current_order);
        } else if (cmd=="QUERY_RANKING"){
            string team; cin >> team;
            if (id.find(team)==id.end()){ cout << "[Error]Query ranking failed: cannot find the team.\n"; continue; }
            cout << "[Info]Complete query ranking.\n";
            if (frozen){ cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n"; }
            int ti = id[team]; int rk = 0; for(size_t i=0;i<last_rank_order.size();++i){ if (last_rank_order[i]==ti){ rk=(int)i+1; break; } }
            cout << team << " NOW AT RANKING " << rk << "\n";
        } else if (cmd=="QUERY_SUBMISSION"){
            string team, WHERE, probEq, AND, statusEq; cin >> team >> WHERE >> probEq >> AND >> statusEq;
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
