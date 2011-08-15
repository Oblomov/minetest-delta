/*
Minetest-c55
Copyright (C) 2010 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef CLIENT_HEADER
#define CLIENT_HEADER

#ifndef SERVER

#include "connection.h"
#include "environment.h"
#include "common_irrlicht.h"
#include "jmutex.h"
#include <ostream>
#include "clientobject.h"
#include "utility.h" // For IntervalLimiter

struct MeshMakeData;

class ClientNotReadyException : public BaseException
{
public:
	ClientNotReadyException(const char *s):
		BaseException(s)
	{}
};

struct QueuedMeshUpdate
{
	v3s16 p;
	MeshMakeData *data;
	bool ack_block_to_server;

	QueuedMeshUpdate();
	~QueuedMeshUpdate();
};

/*
	A thread-safe queue of mesh update tasks
*/
class MeshUpdateQueue
{
public:
	MeshUpdateQueue();

	~MeshUpdateQueue();
	
	/*
		peer_id=0 adds with nobody to send to
	*/
	void addBlock(const v3s16 &p, MeshMakeData *data, bool ack_block_to_server);

	// Returned pointer must be deleted
	// Returns NULL if queue is empty
	QueuedMeshUpdate * pop();

	u32 size() const
	{
		JMutexAutoLock lock(m_mutex);
		return m_queue.size();
	}
	
private:
	core::list<QueuedMeshUpdate*> m_queue;
	mutable JMutex m_mutex;
};

struct MeshUpdateResult
{
	v3s16 p;
	scene::SMesh *mesh;
	bool ack_block_to_server;

	MeshUpdateResult():
		p(-1338,-1338,-1338),
		mesh(NULL),
		ack_block_to_server(false)
	{
	}
};

class MeshUpdateThread : public SimpleThread
{
public:

	MeshUpdateThread()
	{
	}

	void * Thread();

	MeshUpdateQueue m_queue_in;

	MutexedQueue<MeshUpdateResult> m_queue_out;
};

enum ClientEventType
{
	CE_NONE,
	CE_PLAYER_DAMAGE,
	CE_PLAYER_FORCE_MOVE
};

struct ClientEvent
{
	ClientEventType type;
	union{
		struct{
		} none;
		struct{
			u8 amount;
		} player_damage;
		struct{
			f32 pitch;
			f32 yaw;
		} player_force_move;
	};
};

class Client : public con::PeerHandler, public InventoryManager
{
public:
	/*
		NOTE: Nothing is thread-safe here.
	*/

	Client(
			IrrlichtDevice *device,
			const char *playername,
			std::string password,
			MapDrawControl &control
			);
	
	~Client();
	/*
		The name of the local player should already be set when
		calling this, as it is sent in the initialization.
	*/
	void connect(const Address &address);
	/*
		returns true when
			m_con.Connected() == true
			AND m_server_ser_ver != SER_FMT_VER_INVALID
		throws con::PeerNotFoundException if connection has been deleted,
		eg. timed out.
	*/
	bool connectedAndInitialized() const;
	/*
		Stuff that references the environment is valid only as
		long as this is not called. (eg. Players)
		If this throws a PeerNotFoundException, the connection has
		timed out.
	*/
	void step(float dtime);

	// Called from updater thread
	// Returns dtime
	//float asyncStep();

	void ProcessData(u8 *data, u32 datasize, u16 sender_peer_id);
	// Returns true if something was received
	bool AsyncProcessPacket();
	bool AsyncProcessData();
	void Send(u16 channelnum, const SharedBuffer<u8> &data, bool reliable) const;

	// Pops out a packet from the packet queue
	//IncomingPacket getPacket();

	void groundAction(u8 action, const v3s16 &nodepos_undersurface,
			const v3s16 &nodepos_oversurface, u16 item) const;
	void clickObject(u8 button, const v3s16 &blockpos, s16 id, u16 item) const;
	void clickActiveObject(u8 button, u16 id, u16 item) const;

	void sendSignText(const v3s16 &blockpos, s16 id,
			const std::string &text) const;
	void sendSignNodeText(const v3s16 &p, const std::string &text) const;
	void sendInventoryAction(const InventoryAction *a) const;
	void sendChatMessage(const std::wstring &message) const;
	void sendChangePassword(const std::wstring &oldpassword,
		const std::wstring &newpassword) const;
	void sendDamage(u8 damage) const;
	
	// locks envlock
	void removeNode(const v3s16 &p);
	// locks envlock
	void addNode(const v3s16 &p, const MapNode &n);
	
	void updateCamera(const v3f &pos, const v3f &dir);
	
	// Returns InvalidPositionException if not found
	const MapNode &getNode(const v3s16 &p) const;
	// Wrapper to Map
	const NodeMetadata* getNodeMetadata(const v3s16 &p) const;
	NodeMetadata* getNodeMetadata(const v3s16 &p);

	// Get the player position, and optionally put the
	// eye position in *eye_position
	const v3f &getPlayerPosition(v3f *eye_position=NULL) const;

	void setPlayerControl(const PlayerControl &control);

	void selectPlayerItem(u16 item);

	// Returns true if the inventory of the local player has been
	// updated from the server. If it is true, it is set to false.
	bool getLocalInventoryUpdated();
	// Copies the inventory of the local player to parameter
	void getLocalInventory(Inventory &dst);
	
	const InventoryContext *getInventoryContext() const;
	InventoryContext *getInventoryContext();

	const Inventory* getInventory(InventoryContext *c, const std::string &id) const;
	Inventory* getInventory(InventoryContext *c, const std::string &id);
	void inventoryAction(InventoryAction *a);

	// Gets closest object pointed by the shootline
	// Returns NULL if not found
	MapBlockObject * getSelectedObject(
			f32 max_d,
			const v3f &from_pos_f_on_map,
			core::line3d<f32> shootline_on_map
	) const;

	// Gets closest object pointed by the shootline
	// Returns NULL if not found
	ClientActiveObject * getSelectedActiveObject(
			f32 max_d,
			const v3f &from_pos_f_on_map,
			core::line3d<f32> shootline_on_map
	);

	// Prints a line or two of info
	void printDebugInfo(std::ostream &os) const;

	u32 getDayNightRatio() const;

	u16 getHP() const;

	void setTempMod(const v3s16 &p, const NodeMod &mod);
	void clearTempMod(const v3s16 &p);

	float getAvgRtt()
	{
		//JMutexAutoLock lock(m_con_mutex); //bulk comment-out
		con::Peer *peer = m_con.GetPeerNoEx(PEER_ID_SERVER);
		if(peer == NULL)
			return 0.0;
		return peer->avg_rtt;
	}

	bool getChatMessage(std::wstring &message)
	{
		if(m_chat_queue.size() == 0)
			return false;
		message = m_chat_queue.pop_front();
		return true;
	}

	void addChatMessage(const std::wstring &message)
	{
		//JMutexAutoLock envlock(m_env_mutex); //bulk comment-out
		LocalPlayer *player = m_env.getLocalPlayer();
		assert(player != NULL);
		std::wstring name = narrow_to_wide(player->getName());
		m_chat_queue.push_back(
				(std::wstring)L"<"+name+L"> "+message);
	}

	u64 getMapSeed() const { return m_map_seed; }

	void addUpdateMeshTask(const v3s16 &blockpos, bool ack_to_server=false);
	// Including blocks at appropriate edges
	void addUpdateMeshTaskWithEdge(const v3s16 &blockpos, bool ack_to_server=false);

	// Get event from queue. CE_NONE is returned if queue is empty.
	ClientEvent getClientEvent();
	
	inline bool accessDenied() const
	{
		return m_access_denied;
	}

	inline const std::wstring &accessDeniedReason() const
	{
		return m_access_denied_reason;
	}
	
	/*
		This should only be used for calling the special drawing stuff in
		ClientEnvironment
	*/
	const ClientEnvironment * getEnv() const
	{
		return &m_env;
	}

private:
	
	// Virtual methods from con::PeerHandler
	void peerAdded(con::Peer *peer);
	void deletingPeer(con::Peer *peer, bool timeout);
	
	void ReceiveAll();
	void Receive();
	
	void sendPlayerPos() const;
	// This sends the player's current name etc to the server
	void sendPlayerInfo() const;
	// Send the item number 'item' as player item to the server
	void sendPlayerItem(u16 item) const;
	
	float m_packetcounter_timer;
	float m_connection_reinit_timer;
	float m_avg_rtt_timer;
	float m_playerpos_send_timer;
	float m_ignore_damage_timer; // Used after server moves player
	IntervalLimiter m_map_timer_and_unload_interval;

	MeshUpdateThread m_mesh_update_thread;
	
	ClientEnvironment m_env;
	
	con::Connection m_con;

	IrrlichtDevice *m_device;

	v3f camera_position;
	v3f camera_direction;
	
	// Server serialization version
	u8 m_server_ser_ver;

	// This is behind m_env_mutex.
	bool m_inventory_updated;

	core::map<v3s16, bool> m_active_blocks;

	PacketCounter m_packetcounter;
	
	// Received from the server. 0-23999
	u32 m_time_of_day;
	
	// 0 <= m_daynight_i < DAYNIGHT_CACHE_COUNT
	//s32 m_daynight_i;
	//u32 m_daynight_ratio;

	Queue<std::wstring> m_chat_queue;
	
	// The seed returned by the server in TOCLIENT_INIT is stored here
	u64 m_map_seed;
	
	std::string m_password;
	bool m_access_denied;
	std::wstring m_access_denied_reason;

	InventoryContext m_inventory_context;

	Queue<ClientEvent> m_client_event_queue;

	friend class FarMesh;
};

#endif // !SERVER

#endif // !CLIENT_HEADER

