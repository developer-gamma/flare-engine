/*
Copyright © 2011-2012 Clint Bellanger and kitano
Copyright © 2012 Stefan Beller
Copyright © 2012-2016 Justin Jacobs

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class Entity
 *
 * An Entity represents any character in the game - the player, allies, enemies
 * This base class handles logic common to all of these child classes
 */

#include "Animation.h"
#include "AnimationManager.h"
#include "AnimationSet.h"
#include "CampaignManager.h"
#include "CombatText.h"
#include "CommonIncludes.h"
#include "Entity.h"
#include "Hazard.h"
#include "MapRenderer.h"
#include "MessageEngine.h"
#include "PowerManager.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "SoundManager.h"
#include "UtilsMath.h"

#include <math.h>

#ifdef _MSC_VER
#define M_SQRT2 sqrt(2.0)
#endif

const int directionDeltaX[8] =   {-1, -1, -1,  0,  1,  1,  1,  0};
const int directionDeltaY[8] =   { 1,  0, -1, -1, -1,  0,  1,  1};
const float speedMultiplyer[8] = { static_cast<float>(1.0/M_SQRT2), 1.0f, static_cast<float>(1.0/M_SQRT2), 1.0f, static_cast<float>(1.0/M_SQRT2), 1.0f, static_cast<float>(1.0/M_SQRT2), 1.0f};

Entity::Entity()
	: sprites(NULL)
	, sound_attack()
	, sound_hit()
	, sound_die()
	, sound_critdie()
	, sound_block()
	, sound_levelup(0)
	, activeAnimation(NULL)
	, animationSet(NULL) {
}

Entity::Entity(const Entity& e)
	: sprites(e.sprites)
	, sound_attack(e.sound_attack)
	, sound_hit(e.sound_hit)
	, sound_die(e.sound_die)
	, sound_critdie(e.sound_critdie)
	, sound_block(e.sound_block)
	, sound_levelup(e.sound_levelup)
	, activeAnimation(new Animation(*e.activeAnimation))
	, animationSet(e.animationSet)
	, stats(StatBlock(e.stats)) {
}

Entity& Entity::operator=(const Entity& e) {
	if (this == &e)
		return *this;

	sprites = e.sprites;
	sound_attack = e.sound_attack;
	sound_hit = e.sound_hit;
	sound_die = e.sound_die;
	sound_critdie = e.sound_critdie;
	sound_block = e.sound_block;
	sound_levelup = e.sound_levelup;
	activeAnimation = new Animation(*e.activeAnimation);
	animationSet = e.animationSet;
	stats = StatBlock(e.stats);

	return *this;
}

void Entity::loadSounds(StatBlock *src_stats) {
	unloadSounds();

	if (!src_stats) src_stats = &stats;

	for (size_t i = 0; i < src_stats->sfx_attack.size(); ++i) {
		std::string anim_name = src_stats->sfx_attack[i].first;
		sound_attack.push_back(std::pair<std::string, std::vector<SoundID> >());
		sound_attack.back().first = anim_name;
		for (size_t j = 0; j  < src_stats->sfx_attack[i].second.size(); ++j) {
			SoundID sid = snd->load(src_stats->sfx_attack[i].second[j], "Entity attack");
			sound_attack.back().second.push_back(sid);
		}
	}

	for (size_t i = 0; i < src_stats->sfx_hit.size(); ++i) {
		sound_hit.push_back(snd->load(src_stats->sfx_hit[i], "Entity was hit"));
	}
	for (size_t i = 0; i < src_stats->sfx_die.size(); ++i) {
		sound_die.push_back(snd->load(src_stats->sfx_die[i], "Entity died"));
	}
	for (size_t i = 0; i < src_stats->sfx_critdie.size(); ++i) {
		sound_critdie.push_back(snd->load(src_stats->sfx_critdie[i], "Entity died from critical hit"));
	}
	for (size_t i = 0; i < src_stats->sfx_block.size(); ++i) {
		sound_block.push_back(snd->load(src_stats->sfx_block[i], "Entity blocked"));
	}

	if (src_stats->sfx_levelup != "")
		sound_levelup = snd->load(src_stats->sfx_levelup, "Entity leveled up");
}

void Entity::unloadSounds() {
	for (size_t i = 0; i < sound_attack.size(); ++i) {
		for (size_t j = 0; j < sound_attack[i].second.size(); ++j) {
			snd->unload(sound_attack[i].second[j]);
		}
	}

	for (size_t i = 0; i < sound_hit.size(); ++i) {
		snd->unload(sound_hit[i]);
	}
	for (size_t i = 0; i < sound_die.size(); ++i) {
		snd->unload(sound_die[i]);
	}
	for (size_t i = 0; i < sound_critdie.size(); ++i) {
		snd->unload(sound_critdie[i]);
	}
	for (size_t i = 0; i < sound_block.size(); ++i) {
		snd->unload(sound_block[i]);
	}

	snd->unload(sound_levelup);
}

void Entity::playAttackSound(const std::string& attack_name) {
	for (size_t i = 0; i < sound_attack.size(); ++i) {
		if (!sound_attack[i].second.empty() && sound_attack[i].first == attack_name) {
			size_t rand_index = rand() % sound_attack[i].second.size();
			snd->play(sound_attack[i].second[rand_index]);
			return;
		}
	}
}

void Entity::playSound(int sound_type) {
	if (sound_type == ENTITY_SOUND_HIT && !sound_hit.empty()) {
		size_t rand_index = rand() % sound_hit.size();
		std::stringstream channel_name;
		channel_name << "entity_hit_" << sound_hit[rand_index];
		snd->play(sound_hit[rand_index], channel_name.str());
	}
	else if (sound_type == ENTITY_SOUND_DIE && !sound_die.empty()) {
		size_t rand_index = rand() % sound_die.size();
		std::stringstream channel_name;
		channel_name << "entity_die_" << sound_die[rand_index];
		snd->play(sound_die[rand_index], channel_name.str());
	}
	else if (sound_type == ENTITY_SOUND_CRITDIE && !sound_critdie.empty()) {
		size_t rand_index = rand() % sound_critdie.size();
		std::stringstream channel_name;
		channel_name << "entity_critdie_" << sound_critdie[rand_index];
		snd->play(sound_critdie[rand_index], channel_name.str());
	}
	else if (sound_type == ENTITY_SOUND_BLOCK && !sound_block.empty()) {
		size_t rand_index = rand() % sound_block.size();
		std::stringstream channel_name;
		channel_name << "entity_block_" << sound_block[rand_index];
		snd->play(sound_block[rand_index], channel_name.str());
	}
}

void Entity::move_from_offending_tile() {

	// If we got stuck on a tile, which we're not allowed to be on, move away
	// This is just a workaround as we cannot reproduce being stuck easily nor find the
	// errornous code, check https://github.com/clintbellanger/flare-engine/issues/1058

	// As this method should do nothing while regular gameplay, but only in case of bugs
	// we don't need to care about nice graphical effects, so we may just jump out of the
	// offending tile. The idea is simple: We can only be stuck on a tile by accident,
	// so we got here somehow. We'll try to push this entity to the nearest valid place

	FPoint original_pos = stats.pos;
	bool original_pos_is_bad = false;

	while (!mapr->collider.is_valid_position(stats.pos.x, stats.pos.y, stats.movement_type, stats.hero)) {
		original_pos_is_bad = true;

		float pushx = 0;
		float pushy = 0;

		if (mapr->collider.is_valid_position(stats.pos.x + 1, stats.pos.y, stats.movement_type, stats.hero))
			pushx += 0.1f * (2 - (static_cast<float>(static_cast<int>(stats.pos.x + 1)) + 0.5f - stats.pos.x));

		if (mapr->collider.is_valid_position(stats.pos.x - 1, stats.pos.y, stats.movement_type, stats.hero))
			pushx -= 0.1f * (2 - (stats.pos.x - (static_cast<float>(static_cast<int>(stats.pos.x - 1)) + 0.5f)));

		if (mapr->collider.is_valid_position(stats.pos.x, stats.pos.y + 1, stats.movement_type, stats.hero))
			pushy += 0.1f * (2 - (static_cast<float>(static_cast<int>(stats.pos.y + 1)) + 0.5f - stats.pos.y));

		if (mapr->collider.is_valid_position(stats.pos.x, stats.pos.y- 1, stats.movement_type, stats.hero))
			pushy -= 0.1f * (2 - (stats.pos.y - (static_cast<float>(static_cast<int>(stats.pos.y - 1)) + 0.5f)));

		stats.pos.x += pushx;
		stats.pos.y += pushy;

		// we don't move, but we're still stuck on an invalid tile,
		// the final life saver before being crushed by an invalid tile:
		// just blink away. This will seriously irritate the player, but there
		// is probably no other easy way to repair the game
		if (pushx == 0 && pushy == 0) {
			Point src_pos = FPointToPoint(stats.pos);
			FPoint shortest_pos;
			float shortest_dist = 0;
			int radius = 1;

			while (radius <= std::max(mapr->w, mapr->h)) {
				for (int i = src_pos.x - radius; i <= src_pos.x + radius; ++i) {
					for (int j = src_pos.y - radius; j <= src_pos.y + radius; ++j) {
						if (mapr->collider.is_valid_position(static_cast<float>(i), static_cast<float>(j), stats.movement_type, stats.hero)) {
							float test_dist = calcDist(stats.pos, shortest_pos);
							if (shortest_dist == 0 || test_dist < shortest_dist) {
								shortest_dist = test_dist;
								shortest_pos.x = static_cast<float>(i) + 0.5f;
								shortest_pos.y = static_cast<float>(j) + 0.5f;
							}
						}
					}
				}
				if (shortest_dist != 0) {
					stats.pos = shortest_pos;
					break;
				}
				radius++;
			}
		}
	}

	if (original_pos_is_bad) {
		logInfo("Entity: '%s' was stuck and has been moved: (%g, %g) -> (%g, %g)",
				stats.name.c_str(),
				original_pos.x,
				original_pos.y,
				stats.pos.x,
				stats.pos.y);
	}
}

/**
 * move()
 * Apply speed to the direction faced.
 *
 * @return Returns false if wall collision, otherwise true.
 */
bool Entity::move() {

	move_from_offending_tile();

	if (stats.effects.knockback_speed != 0)
		return false;

	if (stats.effects.stun || stats.effects.speed == 0) return false;

	if (stats.charge_speed != 0.0f)
		return false;

	float speed = stats.speed * speedMultiplyer[stats.direction] * stats.effects.speed / 100;
	float dx = speed * static_cast<float>(directionDeltaX[stats.direction]);
	float dy = speed * static_cast<float>(directionDeltaY[stats.direction]);

	bool full_move = mapr->collider.move(stats.pos.x, stats.pos.y, dx, dy, stats.movement_type, stats.hero);

	return full_move;
}

/**
 * Whenever a hazard collides with an entity, this function resolves the effect
 * Called by HazardManager
 *
 * Returns false on miss
 */
bool Entity::takeHit(Hazard &h) {

	//check if this enemy should be affected by this hazard based on the category
	if(!powers->powers[h.power_index].target_categories.empty() && !stats.hero) {
		//the power has a target category requirement, so if it doesnt match, dont continue
		bool match_found = false;
		for (unsigned int i=0; i<stats.categories.size(); i++) {
			if(std::find(powers->powers[h.power_index].target_categories.begin(), powers->powers[h.power_index].target_categories.end(), stats.categories[i]) != powers->powers[h.power_index].target_categories.end()) {
				match_found = true;
			}
		}
		if(!match_found)
			return false;
	}

	// check if this entity allows attacks from this power id
	if (!stats.power_filter.empty() && std::find(stats.power_filter.begin(), stats.power_filter.end(), h.power_index) == stats.power_filter.end()) {
		return false;
	}

	//if the target is already dead, they cannot be hit
	if ((stats.cur_state == ENEMY_DEAD || stats.cur_state == ENEMY_CRITDEAD) && !stats.hero)
		return false;

	if(stats.cur_state == AVATAR_DEAD && stats.hero)
		return false;

	// some attacks will always miss enemies of a certain movement type
	if (stats.movement_type == MOVEMENT_NORMAL && !h.target_movement_normal)
		return false;
	else if (stats.movement_type == MOVEMENT_FLYING && !h.target_movement_flying)
		return false;
	else if (stats.movement_type == MOVEMENT_INTANGIBLE && !h.target_movement_intangible)
		return false;

	// prevent hazard aoe from hitting targets behind walls
	if (h.walls_block_aoe && !mapr->collider.line_of_movement(stats.pos.x, stats.pos.y, h.pos.x, h.pos.y, MOVEMENT_NORMAL))
		return false;

	// some enemies can be invicible based on campaign status
	if (!stats.hero && !stats.hero_ally && h.source_type != SOURCE_TYPE_ENEMY) {
		bool invincible = false;
		for (size_t i = 0; i < stats.invincible_requires_status.size(); ++i) {
			if (!camp->checkStatus(stats.invincible_requires_status[i])) {
				invincible = false;
				break;
			}
			invincible = true;
		}
		if (invincible)
			return false;

		for (size_t i = 0; i < stats.invincible_requires_not_status.size(); ++i) {
			if (camp->checkStatus(stats.invincible_requires_not_status[i])) {
				invincible = false;
				break;
			}
			invincible = true;
		}
		if (invincible)
			return false;
	}

	//if the target is an enemy and they are not already in combat, activate a beacon to draw other enemies into battle
	if (!stats.in_combat && !stats.hero && !stats.hero_ally && !powers->powers[h.power_index].no_aggro) {
		stats.join_combat = true;
	}

	// exit if it was a beacon (to prevent stats.targeted from being set)
	if (powers->powers[h.power_index].beacon) return false;

	// prepare the combat text
	CombatText *combat_text = comb;

	if (h.missile && percentChance(stats.get(STAT_REFLECT))) {
		// reflect the missile 180 degrees
		h.setAngle(h.angle+static_cast<float>(M_PI));

		// change hazard source to match the reflector's type
		// maybe we should change the source stats pointer to the reflector's StatBlock
		if (h.source_type == SOURCE_TYPE_HERO || h.source_type == SOURCE_TYPE_ALLY)
			h.source_type = SOURCE_TYPE_ENEMY;
		else if (h.source_type == SOURCE_TYPE_ENEMY)
			h.source_type = stats.hero ? SOURCE_TYPE_HERO : SOURCE_TYPE_ALLY;

		// reset the hazard ticks
		h.lifespan = h.base_lifespan;

		if (activeAnimation->getName() == "block") {
			playSound(ENTITY_SOUND_BLOCK);
		}

		return false;
	}

	// if it's a miss, do nothing
	int accuracy = h.accuracy;
	if(powers->powers[h.power_index].mod_accuracy_mode == STAT_MODIFIER_MODE_MULTIPLY)
		accuracy = (accuracy * powers->powers[h.power_index].mod_accuracy_value) / 100;
	else if(powers->powers[h.power_index].mod_accuracy_mode == STAT_MODIFIER_MODE_ADD)
		accuracy += powers->powers[h.power_index].mod_accuracy_value;
	else if(powers->powers[h.power_index].mod_accuracy_mode == STAT_MODIFIER_MODE_ABSOLUTE)
		accuracy = powers->powers[h.power_index].mod_accuracy_value;

	int avoidance = 0;
	if(!powers->powers[h.power_index].trait_avoidance_ignore) {
		avoidance = stats.get(STAT_AVOIDANCE);
	}

	int true_avoidance = 100 - (accuracy - avoidance);
	bool is_overhit = (true_avoidance < 0 && !h.src_stats->perfect_accuracy) ? percentChance(abs(true_avoidance)) : false;
	true_avoidance = std::min(std::max(true_avoidance, MIN_AVOIDANCE), MAX_AVOIDANCE);

	bool missed = false;
	if (!h.src_stats->perfect_accuracy && percentChance(true_avoidance)) {
		missed = true;
	}

	// calculate base damage
	int dmg = randBetween(h.dmg_min, h.dmg_max);

	if(powers->powers[h.power_index].mod_damage_mode == STAT_MODIFIER_MODE_MULTIPLY)
		dmg = dmg * powers->powers[h.power_index].mod_damage_value_min / 100;
	else if(powers->powers[h.power_index].mod_damage_mode == STAT_MODIFIER_MODE_ADD)
		dmg += powers->powers[h.power_index].mod_damage_value_min;
	else if(powers->powers[h.power_index].mod_damage_mode == STAT_MODIFIER_MODE_ABSOLUTE)
		dmg = randBetween(powers->powers[h.power_index].mod_damage_value_min, powers->powers[h.power_index].mod_damage_value_max);

	// apply elemental resistance
	if (h.trait_elemental >= 0 && unsigned(h.trait_elemental) < stats.vulnerable.size()) {
		unsigned i = h.trait_elemental;

		int vulnerable = std::max(stats.vulnerable[i], MIN_RESIST);
		if (stats.vulnerable[i] < 100)
			vulnerable = std::min(vulnerable, MAX_RESIST);

		dmg = (dmg * vulnerable) / 100;
	}

	if (!h.trait_armor_penetration) { // armor penetration ignores all absorption
		// subtract absorption from armor
		int absorption = randBetween(stats.get(STAT_ABS_MIN), stats.get(STAT_ABS_MAX));

		if (absorption > 0 && dmg > 0) {
			int abs = absorption;
			if (stats.effects.triggered_block) {
				if ((abs*100)/dmg < MIN_BLOCK)
					absorption = (dmg * MIN_BLOCK) /100;
				if ((abs*100)/dmg > MAX_BLOCK)
					absorption = (dmg * MAX_BLOCK) /100;
				}
			else {
				if ((abs*100)/dmg < MIN_ABSORB)
					absorption = (dmg * MIN_ABSORB) /100;
				if ((abs*100)/dmg > MAX_ABSORB)
					absorption = (dmg * MAX_ABSORB) /100;
			}

			// Sometimes, the absorb limits cause absorbtion to drop to 1
			// This could be confusing to a player that has something with an absorb of 1 equipped
			// So we round absorption up in this case
			if (absorption == 0) absorption = 1;
		}

		dmg = dmg - absorption;
		if (dmg <= 0) {
			dmg = 0;
			if (!powers->powers[h.power_index].ignore_zero_damage) {
				if (h.trait_elemental < 0) {
					if (stats.effects.triggered_block && MAX_BLOCK < 100) dmg = 1;
					else if (!stats.effects.triggered_block && MAX_ABSORB < 100) dmg = 1;
				}
				else {
					if (MAX_RESIST < 100) dmg = 1;
				}
				if (activeAnimation->getName() == "block") {
					playSound(ENTITY_SOUND_BLOCK);
					resetActiveAnimation();
				}
			}
		}
	}

	// check for crits
	int true_crit_chance = h.crit_chance;

	if(powers->powers[h.power_index].mod_crit_mode == STAT_MODIFIER_MODE_MULTIPLY)
		true_crit_chance = true_crit_chance * powers->powers[h.power_index].mod_crit_value / 100;
	else if(powers->powers[h.power_index].mod_crit_mode == STAT_MODIFIER_MODE_ADD)
		true_crit_chance += powers->powers[h.power_index].mod_crit_value;
	else if(powers->powers[h.power_index].mod_crit_mode == STAT_MODIFIER_MODE_ABSOLUTE)
		true_crit_chance = powers->powers[h.power_index].mod_crit_value;

	if (stats.effects.stun || stats.effects.speed < 100)
		true_crit_chance += h.trait_crits_impaired;

	bool crit = percentChance(true_crit_chance);
	if (crit) {
		// default is dmg * 2
		dmg = (dmg * randBetween(MIN_CRIT_DAMAGE, MAX_CRIT_DAMAGE)) / 100;
		if(!stats.hero)
			mapr->shaky_cam_ticks = MAX_FRAMES_PER_SEC/2;
	}
	else if (is_overhit) {
		dmg = (dmg * randBetween(MIN_OVERHIT_DAMAGE, MAX_OVERHIT_DAMAGE)) / 100;
		// Should we use shakycam for overhits?
	}

	// misses cause reduced damage
	if (missed) {
		dmg = (dmg * randBetween(MIN_MISS_DAMAGE, MAX_MISS_DAMAGE)) / 100;
	}

	if (!powers->powers[h.power_index].ignore_zero_damage) {
		if (dmg == 0) {
			combat_text->addString(msg->get("miss"), stats.pos, COMBAT_MESSAGE_MISS);
			return false;
		}
		else if(stats.hero)
			combat_text->addInt(dmg, stats.pos, COMBAT_MESSAGE_TAKEDMG);
		else {
			if(crit || is_overhit)
				combat_text->addInt(dmg, stats.pos, COMBAT_MESSAGE_CRIT);
			else if (missed)
				combat_text->addInt(dmg, stats.pos, COMBAT_MESSAGE_MISS);
			else
				combat_text->addInt(dmg, stats.pos, COMBAT_MESSAGE_GIVEDMG);
		}
	}

	// temporarily save the current HP for calculating HP/MP steal on final blow
	int prev_hp = stats.hp;

	// save debuff status to check for on_debuff powers later
	bool was_debuffed = stats.effects.isDebuffed();

	// apply damage
	stats.takeDamage(dmg);

	// after effects
	if (dmg > 0 || powers->powers[h.power_index].ignore_zero_damage) {

		// damage always breaks stun
		stats.effects.removeEffectType(EFFECT_STUN);

		powers->effect(&stats, h.src_stats, h.power_index,h.source_type);

		// HP/MP steal is cumulative between stat bonus and power bonus
		int hp_steal = h.hp_steal + h.src_stats->get(STAT_HP_STEAL);
		if (!stats.effects.immunity_hp_steal && hp_steal != 0) {
			int steal_amt = (std::min(dmg, prev_hp) * hp_steal) / 100;
			if (steal_amt == 0) steal_amt = 1;
			combat_text->addString(msg->get("+%d HP",steal_amt), h.src_stats->pos, COMBAT_MESSAGE_BUFF);
			h.src_stats->hp = std::min(h.src_stats->hp + steal_amt, h.src_stats->get(STAT_HP_MAX));
		}
		int mp_steal = h.mp_steal + h.src_stats->get(STAT_MP_STEAL);
		if (!stats.effects.immunity_mp_steal && mp_steal != 0) {
			int steal_amt = (std::min(dmg, prev_hp) * mp_steal) / 100;
			if (steal_amt == 0) steal_amt = 1;
			combat_text->addString(msg->get("+%d MP",steal_amt), h.src_stats->pos, COMBAT_MESSAGE_BUFF);
			h.src_stats->mp = std::min(h.src_stats->mp + steal_amt, h.src_stats->get(STAT_MP_MAX));
		}

		// deal return damage
		if (!h.src_stats->effects.immunity_damage_reflect && stats.get(STAT_RETURN_DAMAGE) > 0) {
			int dmg_return = static_cast<int>(static_cast<float>(dmg * stats.get(STAT_RETURN_DAMAGE)) / 100.f);

			if (dmg_return == 0)
				dmg_return = 1;

			h.src_stats->takeDamage(dmg_return);
			comb->addInt(dmg_return, h.src_stats->pos, COMBAT_MESSAGE_GIVEDMG);
		}
	}

	if (dmg > 0 || powers->powers[h.power_index].ignore_zero_damage) {
		// remove effect by ID
		stats.effects.removeEffectID(powers->powers[h.power_index].remove_effects);

		// post power
		if (h.post_power > 0 && percentChance(h.post_power_chance)) {
			powers->activate(h.post_power, h.src_stats, stats.pos);
		}
	}

	// interrupted to new state
	if (dmg > 0) {
		bool chance_poise = percentChance(stats.get(STAT_POISE));

		if(stats.hp <= 0) {
			stats.effects.triggered_death = true;
			if(stats.hero)
				stats.cur_state = AVATAR_DEAD;
			else {
				doRewards(h.source_type);
				if (crit)
					stats.cur_state = ENEMY_CRITDEAD;
				else
					stats.cur_state = ENEMY_DEAD;
				mapr->collider.unblock(stats.pos.x,stats.pos.y);
			}

			return true;
		}

		// play hit sound effect, but only if the hit cooldown is done
		if (stats.cooldown_hit_ticks == 0)
			playSound(ENTITY_SOUND_HIT);

		// if this hit caused a debuff, activate an on_debuff power
		if (!was_debuffed && stats.effects.isDebuffed()) {
			AIPower* ai_power = stats.getAIPower(AI_POWER_DEBUFF);
			if (ai_power != NULL) {
				stats.cur_state = ENEMY_POWER;
				stats.activated_power = ai_power;
				stats.cooldown_ticks = 0; // ignore global cooldown
				return true;
			}
		}

		// roll to see if the enemy's ON_HIT power is casted
		AIPower* ai_power = stats.getAIPower(AI_POWER_HIT);
		if (ai_power != NULL) {
			stats.cur_state = ENEMY_POWER;
			stats.activated_power = ai_power;
			stats.cooldown_ticks = 0; // ignore global cooldown
			return true;
		}

		// don't go through a hit animation if stunned or successfully poised
		// however, critical hits ignore poise
		if(stats.cooldown_hit_ticks == 0) {
			stats.cooldown_hit_ticks = stats.cooldown_hit;

			if (!stats.effects.stun && (!chance_poise || crit) && !stats.prevent_interrupt) {
				if(stats.hero) {
					stats.cur_state = AVATAR_HIT;
				}
				else {
					if (stats.cur_state == ENEMY_POWER) {
						stats.cooldown_ticks = stats.cooldown;
						stats.activated_power = NULL;
					}
					stats.cur_state = ENEMY_HIT;
				}

				if (stats.untransform_on_hit)
					stats.transform_duration = 0;
			}
		}
	}

	return true;
}

void Entity::resetActiveAnimation() {
	activeAnimation->reset();
}

/**
 * Set the entity's current animation by name
 */
bool Entity::setAnimation(const std::string& animationName) {

	// if the animation is already the requested one do nothing
	if (activeAnimation != NULL && activeAnimation->getName() == animationName)
		return true;

	delete activeAnimation;
	activeAnimation = animationSet->getAnimation(animationName);

	if (activeAnimation == NULL)
		logError("Entity::setAnimation(%s): not found", animationName.c_str());

	return activeAnimation == NULL;
}

Entity::~Entity () {
	delete activeAnimation;
}

