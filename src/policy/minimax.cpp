#include <utility>
#include <algorithm>
#include "state.hpp"
#include "minimax.hpp"

static int history_table[2][64][64] = {0};

/*============================================================
 * MiniMax — quiescene
 *============================================================*/
int MiniMax::quiescence(
    State *state,
    SearchContext& ctx,
    const MMParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;
    
    if ((ctx.nodes & 2047) == 0 && ctx.stop) {
        return alpha;
    }
    if (ctx.stop) return alpha;

    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, nullptr);
    if (stand_pat >= beta){
        return beta;
    }
    if (stand_pat > alpha){
        alpha = stand_pat;
    }
    if (state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [state](const Move& a, const Move& b){
        return state->score_move(a) > state->score_move(b);
    });

    for (auto& action : state->legal_actions){
        int next_r = action.second.first;
        int next_c = action.second.second;
        bool is_capture = (state->board.board[1-state->player][next_r][next_c] > 0);
        if (!is_capture) continue;

        State *next = state->next_state(action);
        int score = -quiescence(next, ctx, p, -beta, -alpha);
        delete next;
        
        if (ctx.stop) return alpha; 
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

/*============================================================
 * MiniMax — eval_ctx
 *============================================================*/
int MiniMax::eval_ctx(
    State *state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }

    if ((ctx.nodes & 2047) == 0 && ctx.stop) {
        return alpha;
    }
    if(ctx.stop){
        return alpha;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */
    if(state->game_state == WIN){
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    if(depth <= 0){
        return quiescence(state, ctx, p, alpha, beta);
    }
    history.push(state->hash());

    int current_player = state->player;
    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [state, current_player](const Move& a, const Move& b){
        int score_a = state->score_move(a) * 1000 + history_table[current_player][a.first.first * 6 + a.first.second][a.second.first * 6 + a.second.second];
        int score_b = state->score_move(b) * 1000 + history_table[current_player][b.first.first * 6 + b.first.second][b.second.first * 6 + b.second.second];
        return score_a > score_b;
    });

    int best_score = M_MAX - 1000; 
    bool first_move = true;
    Move best_move_this_node;

    for(auto& action : state->legal_actions){
        if (ctx.stop) break;

        State *next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int score;
/*============================================================
 * MiniMax — PVS
 *============================================================*/

        if (first_move){
            int raw_score = eval_ctx(next, depth-1, history, ply+1, ctx, p, -beta, -alpha);
            score = same ? raw_score : -raw_score;
            first_move = false;
        } else{
            int raw_score = eval_ctx(next, depth-1, history, ply+1, ctx, p, -alpha-1, -alpha);
            score = same ? raw_score : -raw_score;
            if (score > alpha && score < beta && !ctx.stop){
                raw_score = eval_ctx(next, depth-1, history, ply+1, ctx, p, -beta, -alpha);
                score = same ? raw_score : -raw_score;
            }
        }

        delete next;

        if (ctx.stop) break; 

        if(score > best_score){
            best_score = score;
            best_move_this_node = action;
        }

        if (score > alpha){
            alpha = score;
        }
        if (alpha >= beta){
            int from_idx = action.first.first * 6 + action.first.second;
            int to_idx = action.second.first * 6 + action.second.second;
            history_table[current_player][from_idx][to_idx] += depth * depth;
            break;
        }
    }

    history.pop(state->hash());
    return ctx.stop ? alpha : best_score;
}

/*============================================================
 * MiniMax — search
 *============================================================*/
SearchResult MiniMax::search(
    State *state,
    int depth, 
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult global_best_result;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }
    
    if(!state->legal_actions.empty()){
        global_best_result.best_move = state->legal_actions[0];
    }
    global_best_result.score = M_MAX - 10;
    global_best_result.depth = 0;

    for(int pl=0; pl<2; ++pl)
        for(int f=0; f<35; ++f)
            for(int t=0; t<35; ++t)
                history_table[pl][f][t] /= 2;

    int max_search_depth = std::max(depth, 64); 

    for (int current_depth = 1; current_depth <= max_search_depth; ++current_depth) {
        
        if (ctx.stop) break; 

        int current_player = state->player;
        std::sort(state->legal_actions.begin(), state->legal_actions.end(), [state, current_player](const Move& a, const Move& b){
            int score_a = state->score_move(a) * 1000 + history_table[current_player][a.first.first * 6 + a.first.second][a.second.first * 6 + a.second.second];
            int score_b = state->score_move(b) * 1000 + history_table[current_player][b.first.first * 6 + b.first.second][b.second.first * 6 + b.second.second];
            return score_a > score_b;
        });
        
        if (current_depth > 1) {
            auto it = std::find(state->legal_actions.begin(), state->legal_actions.end(), global_best_result.best_move);
            if (it != state->legal_actions.end()) {
                std::rotate(state->legal_actions.begin(), it, it + 1);
            }
        }

        int alpha = M_MAX - 100;
        int beta = P_MAX + 100;
        int best_score = M_MAX - 1000; 
        int move_index = 0;
        int total_moves = (int)state->legal_actions.size();
        bool first_move = true;

        Move current_depth_best_move;
        bool iteration_completed = true;

        for(auto& action : state->legal_actions){
            if (ctx.stop) {
                iteration_completed = false;
                break;
            }

            State *next = state->next_state(action);
            history.push(state->hash());

            bool same = next->same_player_as_parent();
            int score;
/*============================================================
 * MiniMax — PVS
 *============================================================*/
            if (first_move){
                int raw_score = eval_ctx(next, current_depth-1, history, 1, ctx, p, -beta, -alpha);
                score = same ? raw_score : -raw_score;
                first_move = false;
            } else{
                int raw_score = eval_ctx(next, current_depth-1, history, 1, ctx, p, -alpha-1, -alpha);
                score = same ? raw_score : -raw_score;

                if (score > alpha && score < beta && !ctx.stop){
                    raw_score = eval_ctx(next, current_depth-1, history, 1, ctx, p, -beta, -alpha);
                    score = same ? raw_score : -raw_score;
                }
            }

            history.pop(state->hash());
            delete next;

            if (ctx.stop) {
                iteration_completed = false;
                break;
            }

            if(score > best_score){
                best_score = score;
                current_depth_best_move = action;   
                
                if(p.report_partial && ctx.on_root_update){
                   ctx.on_root_update({current_depth_best_move, best_score, current_depth, move_index + 1, total_moves});
                }
            } 
            if (score > alpha){
                alpha = score;
            }
            move_index++;
        }

        if (iteration_completed && !ctx.stop) {
            global_best_result.score = best_score;
            global_best_result.best_move = current_depth_best_move;
            global_best_result.depth = current_depth;
        } else {
            break; 
        }
    }

    return global_best_result;
} 

/*============================================================
 * MiniMax — default_params / param_defs
 *============================================================*/
ParamMap MiniMax::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> MiniMax::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}