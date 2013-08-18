#include "Player.h"

#include <algorithm>

#ifndef VAULTSERVER
#include "Game.h"
#endif

using namespace std;
using namespace RakNet;
using namespace Values;

#ifdef VAULTSERVER
Guarded<Player::BaseIDTracker> Player::baseIDs;
Guarded<Player::WindowTracker> Player::attachedWindows;

atomic<unsigned int> Player::default_respawn(DEFAULT_PLAYER_RESPAWN);
atomic<unsigned int> Player::default_cell;
atomic<bool> Player::default_console(true);
#endif

const map<unsigned char, pair<double, double>> Player::default_values = {
	{ActorVal_Energy, {50.0, 50.0}},
	{ActorVal_Responsibility, {50.0, 50.0}},
	{ActorVal_Strength, {5.0, 5.0}},
	{ActorVal_Perception, {5.0, 5.0}},
	{ActorVal_Endurance, {5.0, 5.0}},
	{ActorVal_Charisma, {5.0, 5.0}},
	{ActorVal_Intelligence, {5.0, 5.0}},
	{ActorVal_Agility, {5.0, 5.0}},
	{ActorVal_Luck, {5.0, 5.0}},
	{ActorVal_ActionPoints, {75.0, 75.0}},
	{ActorVal_CarryWeight, {200.0, 200.0}},
	{ActorVal_CritChance, {5.0, 5.0}},
	{ActorVal_Health, {200.0, 200.0}},
	{ActorVal_MeleeDamage, {2.0, 2.0}},
	{ActorVal_PoisonResistance, {20.0, 20.0}},
	{ActorVal_RadResistance, {8.0, 8.0}},
	{ActorVal_SpeedMultiplier, {100.0, 100.0}},
	{ActorVal_Fatigue, {200.0, 200.0}},
	{ActorVal_Head, {100.0, 100.0}},
	{ActorVal_Torso, {100.0, 100.0}},
	{ActorVal_LeftArm, {100.0, 100.0}},
	{ActorVal_RightArm, {100.0, 100.0}},
	{ActorVal_LeftLeg, {100.0, 100.0}},
	{ActorVal_RightLeg, {100.0, 100.0}},
	{ActorVal_Brain, {100.0, 100.0}},
	{ActorVal_Barter, {15.0, 15.0}},
	{ActorVal_BigGuns, {15.0, 15.0}},
	{ActorVal_EnergyWeapons, {15.0, 15.0}},
	{ActorVal_Explosives, {15.0, 15.0}},
	{ActorVal_Lockpick, {15.0, 15.0}},
	{ActorVal_Medicine, {15.0, 15.0}},
	{ActorVal_MeleeWeapons, {15.0, 15.0}},
	{ActorVal_Repair, {15.0, 15.0}},
	{ActorVal_Science, {15.0, 15.0}},
	{ActorVal_SmallGuns, {15.0, 15.0}},
	{ActorVal_Sneak, {15.0, 15.0}},
	{ActorVal_Speech, {15.0, 15.0}},
	{ActorVal_Unarmed, {15.0, 15.0}},
	{ActorVal_UnarmedDamage, {1.0, 1.25}},
};

#ifdef VAULTMP_DEBUG
DebugInput<Player> Player::debug;
#endif

Player::Player(unsigned int refID, unsigned int baseID) : Actor(refID, baseID)
{
	initialize();

	const auto& values = default_values;

	for (const auto& value : values)
	{
		this->SetActorBaseValue(value.first, value.second.first);
		this->SetActorValue(value.first, value.second.second);
	}

	this->SetActorRace(RACE_CAUCASIAN);
	this->SetActorFemale(false);
}

Player::Player(const pDefault* packet) : Actor(PacketFactory::Pop<pPacket>(packet))
{
	initialize();

	map<unsigned char, pair<unsigned char, bool>> controls;

	PacketFactory::Access<pTypes::ID_PLAYER_NEW>(packet, controls);

	for (const auto& control : controls)
	{
		this->SetPlayerControl(control.first, control.second.first);
		this->SetPlayerControlEnabled(control.first, control.second.second);
	}
}

Player::~Player() noexcept
{
#ifdef VAULTMP_DEBUG
	debug.print("Player object destroyed (ref: ", hex, this->GetReference(), ")");
#endif

#ifdef VAULTSERVER
	baseIDs.Operate([this](BaseIDTracker& baseIDs) {
		baseIDs.erase(find(baseIDs.begin(), baseIDs.end(), this->GetBase()));
	});

	attachedWindows.Operate([this](WindowTracker& attachedWindows) {
		for (const auto& id : *player_Windows)
			attachedWindows[id].erase(find(attachedWindows[id].begin(), attachedWindows[id].end(), this->GetNetworkID()));
	});
#endif
}

void Player::initialize()
{
	vector<unsigned char> data = API::RetrieveAllControls();

	for (unsigned char _data : data)
		player_Controls.insert(make_pair(_data, make_pair(Value<unsigned char>(), Value<bool>(true))));

#ifdef VAULTSERVER
	baseIDs.Operate([this](BaseIDTracker& baseIDs) {
		baseIDs.emplace_back(this->GetBase());
	});

	player_Respawn.set(default_respawn);
	player_Cell.set(default_cell);
	state_Console.set(default_console);
#endif
}

#ifdef VAULTSERVER
unsigned int Player::GetRespawnTime()
{
	return default_respawn;
}

unsigned int Player::GetSpawnCell()
{
	return default_cell;
}

bool Player::GetConsoleEnabled()
{
	return default_console;
}

void Player::SetRespawnTime(unsigned int respawn)
{
	default_respawn = respawn;
}

void Player::SetSpawnCell(unsigned int cell)
{
	if (!DB::Record::IsValidCell(cell))
		throw VaultException("%08X is not a valid cell", cell).stacktrace();

	default_cell = cell;
}

void Player::SetConsoleEnabled(bool enabled)
{
	default_console = enabled;
}
#endif

#ifndef VAULTSERVER
FuncParameter Player::CreateFunctor(unsigned int flags, NetworkID id)
{
	return FuncParameter(unique_ptr<VaultFunctor>(new PlayerFunctor(flags, id)));
}
#endif

unsigned char Player::GetPlayerControl(unsigned char control) const
{
	return player_Controls.at(control).first.get();
}

bool Player::GetPlayerControlEnabled(unsigned char control) const
{
	return player_Controls.at(control).second.get();
}

#ifdef VAULTSERVER
unsigned int Player::GetPlayerRespawnTime() const
{
	return player_Respawn.get();
}

unsigned int Player::GetPlayerSpawnCell() const
{
	return player_Cell.get();
}

const Player::CellContext& Player::GetPlayerCellContext() const
{
	return *player_CellContext;
}

bool Player::GetPlayerConsoleEnabled() const
{
	return state_Console.get();
}
#endif

Lockable* Player::SetPlayerControl(unsigned char control, unsigned char key)
{
	return SetObjectValue(this->player_Controls.at(control).first, key);
}

Lockable* Player::SetPlayerControlEnabled(unsigned char control, bool state)
{
	return SetObjectValue(this->player_Controls.at(control).second, state);
}

#ifdef VAULTSERVER
Lockable* Player::SetPlayerRespawnTime(unsigned int respawn)
{
	return SetObjectValue(this->player_Respawn, respawn);
}

Lockable* Player::SetPlayerSpawnCell(unsigned int cell)
{
	if (!DB::Record::IsValidCell(cell))
		throw VaultException("%08X is not a valid cell", cell).stacktrace();

	return SetObjectValue(this->player_Cell, cell);
}

Lockable* Player::SetNetworkCell(unsigned int cell)
{
	Lockable* ret = Object::SetNetworkCell(cell);

	if (ret)
	{
		auto new_exterior = DB::Exterior::Lookup(cell);

		if (new_exterior)
			*player_CellContext = new_exterior->GetAdjacents();
		else
			*player_CellContext = {{cell, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}};
	}

	return ret;
}

Lockable* Player::SetPlayerConsoleEnabled(bool enabled)
{
	return SetObjectValue(this->state_Console, enabled);
}

Lockable* Player::AttachWindow(RakNet::NetworkID id)
{
	if (find(player_Windows->begin(), player_Windows->end(), id) == player_Windows->end())
	{
		player_Windows->emplace_back(id);

		attachedWindows.Operate([this, id](WindowTracker& attachedWindows) {
			attachedWindows[id].emplace_back(this->GetNetworkID());
		});

		return &player_Windows;
	}

	return nullptr;
}

Lockable* Player::DetachWindow(RakNet::NetworkID id)
{
	AttachedWindows::iterator it;

	if ((it = find(player_Windows->begin(), player_Windows->end(), id)) != player_Windows->end())
	{
		player_Windows->erase(it);

		attachedWindows.Operate([this, id](WindowTracker& attachedWindows) {
			attachedWindows[id].erase(find(attachedWindows[id].begin(), attachedWindows[id].end(), this->GetNetworkID()));
		});

		return &player_Windows;
	}

	return nullptr;
}
#endif

#ifdef VAULTSERVER
Lockable* Player::SetBase(unsigned int baseID)
{
	unsigned int prev_baseID = this->GetBase();
	auto ret = Actor::SetBase(baseID);

	if (ret)
		baseIDs.Operate([prev_baseID, baseID](BaseIDTracker& baseIDs) {
			*find(baseIDs.begin(), baseIDs.end(), prev_baseID) = baseID;
		});

	return ret;
}
#endif

pPacket Player::toPacket() const
{
	vector<unsigned char> data = API::RetrieveAllControls();
	map<unsigned char, pair<unsigned char, bool>> controls;

	for (unsigned char _data : data)
		controls.insert(make_pair(_data, make_pair(this->GetPlayerControl(_data), this->GetPlayerControlEnabled(_data))));

	pPacket pActorNew = Actor::toPacket();
	pPacket packet = PacketFactory::Create<pTypes::ID_PLAYER_NEW>(pActorNew, controls);

	return packet;
}

#ifndef VAULTSERVER
vector<string> PlayerFunctor::operator()()
{
	vector<string> result;
	NetworkID id = get();

	if (id)
		GameFactory::Operate<Player>(id, [this, &result](FactoryPlayer& player) {
			unsigned int flags = this->flags();

			if (flags & FLAG_MOVCONTROLS)
			{
				unsigned int forward, backward, left, right;

				forward = player->GetPlayerControl(Values::ControlCode_Forward);
				backward = player->GetPlayerControl(Values::ControlCode_Backward);
				left = player->GetPlayerControl(Values::ControlCode_Left);
				right = player->GetPlayerControl(Values::ControlCode_Right);

				unsigned int movcontrols = (right | (left << 8) | (backward << 16) | (forward << 24));

				result.emplace_back(Utils::toString(movcontrols));
			}
		});
	else
	{
		auto references = Game::GetContext(ID_PLAYER);

		for (unsigned int refID : references)
			GameFactory::Operate<Player, FailPolicy::Return>(refID, [this, refID, &result](FactoryPlayer& player) {
				if (!filter(player))
					result.emplace_back(Utils::toString(refID));
			});
	}

	_next(result);

	return result;
}

bool PlayerFunctor::filter(FactoryWrapper<Reference>& reference)
{
	if (ActorFunctor::filter(reference))
		return true;

	return GameFactory::Operate<Player>(reference->GetNetworkID(), [this](FactoryPlayer& player) {
		unsigned int flags = this->flags();

		if (flags & FLAG_SELFALERT)
		{
			if (!player->GetActorAlerted())
				return true;
		}

		return false;
	});
}
#endif
