#include <utility>
#include <algorithm>
#include "state.hpp"
#include "minimax.hpp"

int MiniMax::quiescene(
    State *state,
    SearchContext& ctx,
    const MMParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;
    
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
        int score = -quiescene(next, ctx, p, -beta, -alpha);
        delete next;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}
/*============================================================
 * MiniMax — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
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
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */

    // [ Hackathon TODO 3-1 ]
    // return the score for a winning terminal state
    // Hint: prefer faster wins by using ply.
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
        return quiescene(state, ctx, p, alpha, beta);
    }
    history.push(state->hash());

    /* === Negamax loop === */
    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [state](const Move& a, const Move& b){
        return state->score_move(a) > state->score_move(b);
    });
    int best_score = M_MAX;
    bool first_move = true;

    for(auto& action : state->legal_actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action
        State *next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int score;

        if (first_move){
            int raw_score = eval_ctx(next, depth-1, history, ply+1, ctx, p, -beta, -alpha);
            score = same ? raw_score : -raw_score;
            first_move = false;
        } else{
            int raw_score = eval_ctx(next, depth-1, history, ply+1, ctx, p, -alpha-1, -alpha);
            score = same ? raw_score : -raw_score;
            if (score > alpha && score < beta){
                raw_score = eval_ctx(next, depth-1, history, ply+1, ctx, p, -beta, -alpha);
                score = same ? raw_score : -raw_score;
            }
        }

        // [Hackathon TODO 3-3]
        // search the child one level deeper

        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.

        delete next;

        // [ Hackathon TODO 3-5 ]
        // update best_score if this child is better.
        if(score > best_score){
            best_score = score;
        }

        if (score > alpha){
            alpha = score;
        }
        if (alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * MiniMax — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult MiniMax::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [state](const Move& a, const Move& b){
        return state->score_move(a) > state->score_move(b);
    });
    
    int alpha = M_MAX - 100;
    int beta = P_MAX + 100;
    int best_score = M_MAX - 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    bool first_move = true;

    for(auto& action : state->legal_actions){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
        State *next = state->next_state(action);

        bool same = next->same_player_as_parent();
        int score;
        
        if (first_move){
            int raw_score = eval_ctx(next, depth-1, history, 1, ctx, p, -beta, -alpha);
            score = same ? raw_score : -raw_score;
            first_move = false;
        } else{
            int raw_score = eval_ctx(next, depth-1, history, 1, ctx, p, -alpha-1, -alpha);
            score = same ? raw_score : -raw_score;

            if (score > alpha && score < beta){
                raw_score = eval_ctx(next, depth-1, history, 1, ctx, p, -beta, -alpha);
                score = same ? raw_score : -raw_score;
            }
        }

        delete next;

        if(score > best_score){
            // [ Hackathon TODO 4-2 ]
            // keep this move if it is the best so far
            best_score = score;
            result.best_move = action;    

            if(p.report_partial && ctx.on_root_update){
               ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        } 
        if (score > alpha){
            alpha = score;
        }
        move_index++;
    }

    // [ Hackathon TODO 4-3 ]
    // update result and return
    result.score = best_score;
    return result;
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