#pragma once

#include <exception>
#include <optional>

class InvalidOperation : public std::exception {
public:
    const char* what() const noexcept override { return "invalid operation"; }
};

struct PlayInfo {
    int dummyCount = 0;
    int magnifierCount = 0;
    int converterCount = 0;
    int cageCount = 0;
};

class GameState {
public:
    enum class BulletType { Live, Blank };
    enum class ItemType { Dummy, Magnifier, Converter, Cage };

    GameState()
        : currentPlayer_(0), liveCount_(0), blankCount_(0) {
        hp_[0] = 5;
        hp_[1] = 5;
        usedCageThisTurn_[0] = false;
        usedCageThisTurn_[1] = false;
        cageActive_[0] = false;
        cageActive_[1] = false;
    }

    void fireAtOpponent(BulletType topBulletBeforeAction) {
        if (isOver_()) return;  // no-op if already over
        consumeTopBullet_(topBulletBeforeAction);
        if (topBulletBeforeAction == BulletType::Live) {
            hp_[opponent_()] -= 1;
        }
        if (!isOver_()) {
            endTurnAfterShot_();
        }
    }

    void fireAtSelf(BulletType topBulletBeforeAction) {
        if (isOver_()) return;
        consumeTopBullet_(topBulletBeforeAction);
        if (topBulletBeforeAction == BulletType::Live) {
            hp_[currentPlayer_] -= 1;
            if (!isOver_()) {
                endTurnAfterShot_();
            }
        } else {
            // Blank: continue the same turn, nothing else
        }
    }

    void useDummy(BulletType topBulletBeforeUse) {
        if (players_[currentPlayer_].dummyCount <= 0) {
            throw InvalidOperation();
        }
        // consume the item first only after checks pass
        players_[currentPlayer_].dummyCount -= 1;
        // consume the top bullet, revealing its type
        consumeTopBullet_(topBulletBeforeUse);
    }

    void useMagnifier(BulletType topBulletBeforeUse) {
        if (players_[currentPlayer_].magnifierCount <= 0) {
            throw InvalidOperation();
        }
        players_[currentPlayer_].magnifierCount -= 1;
        // Reveal next bullet type to everyone (deterministic knowledge)
        knownNext_ = topBulletBeforeUse;
    }

    void useConverter(BulletType topBulletBeforeUse) {
        if (players_[currentPlayer_].converterCount <= 0) {
            throw InvalidOperation();
        }
        players_[currentPlayer_].converterCount -= 1;
        // Flip the next bullet: update counts and knowledge accordingly
        if (topBulletBeforeUse == BulletType::Live) {
            // live -> blank
            if (liveCount_ > 0) {
                --liveCount_;
                ++blankCount_;
            }
            knownNext_ = BulletType::Blank;
        } else {
            // blank -> live
            if (blankCount_ > 0) {
                --blankCount_;
                ++liveCount_;
            }
            knownNext_ = BulletType::Live;
        }
    }

    void useCage() {
        if (usedCageThisTurn_[currentPlayer_]) {
            throw InvalidOperation();
        }
        if (players_[currentPlayer_].cageCount <= 0) {
            throw InvalidOperation();
        }
        players_[currentPlayer_].cageCount -= 1;
        usedCageThisTurn_[currentPlayer_] = true;
        cageActive_[currentPlayer_] = true;
    }

    void reloadBullets(int liveCount, int blankCount) {
        liveCount_ = liveCount;
        blankCount_ = blankCount;
        knownNext_.reset();
    }

    void reloadItem(int playerId, ItemType item) {
        if (playerId != 0 && playerId != 1) return;
        PlayInfo& p = players_[playerId];
        switch (item) {
            case ItemType::Dummy:     ++p.dummyCount; break;
            case ItemType::Magnifier: ++p.magnifierCount; break;
            case ItemType::Converter: ++p.converterCount; break;
            case ItemType::Cage:      ++p.cageCount; break;
        }
    }

    double nextLiveBulletProbability() const {
        if (knownNext_.has_value()) {
            return (*knownNext_ == BulletType::Live) ? 1.0 : 0.0;
        }
        const int total = liveCount_ + blankCount_;
        if (total <= 0) return 0.0;
        return static_cast<double>(liveCount_) / static_cast<double>(total);
    }

    double nextBlankBulletProbability() const {
        if (knownNext_.has_value()) {
            return (*knownNext_ == BulletType::Blank) ? 1.0 : 0.0;
        }
        const int total = liveCount_ + blankCount_;
        if (total <= 0) return 0.0;
        return static_cast<double>(blankCount_) / static_cast<double>(total);
    }

    int winnerId() const {
        if (hp_[0] <= 0 && hp_[1] <= 0) {
            // Should not occur in this game, but tie-breaker: no winner specified.
            return -1;
        }
        if (hp_[0] <= 0) return 1;
        if (hp_[1] <= 0) return 0;
        return -1;
    }

private:
    int opponent_() const { return 1 - currentPlayer_; }

    bool isOver_() const { return hp_[0] <= 0 || hp_[1] <= 0; }

    void consumeTopBullet_(BulletType type) {
        // consume bullet from counts based on actual provided type
        if (type == BulletType::Live) {
            if (liveCount_ > 0) --liveCount_;
        } else {
            if (blankCount_ > 0) --blankCount_;
        }
        // knowledge about next bullet becomes stale after consumption
        knownNext_.reset();
    }

    void endTurnAfterShot_() {
        if (cageActive_[currentPlayer_]) {
            // negate exactly one end-turn; effect consumed
            cageActive_[currentPlayer_] = false;
            return;
        }
        // switch turn
        const int old = currentPlayer_;
        currentPlayer_ = opponent_();
        // starting a new player's turn resets their per-turn cage usage flag
        usedCageThisTurn_[currentPlayer_] = false;
        // ensure old player's cage effect is cleared when their turn ends
        cageActive_[old] = false;
    }

private:
    int hp_[2];
    int currentPlayer_;
    int liveCount_;
    int blankCount_;
    std::optional<BulletType> knownNext_;
    PlayInfo players_[2];
    bool usedCageThisTurn_[2];
    bool cageActive_[2];
};

