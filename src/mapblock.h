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

#ifndef MAPBLOCK_HEADER
#define MAPBLOCK_HEADER

#include <jmutex.h>
#include <jmutexautolock.h>
#include <exception>
#include "debug.h"
#include "common_irrlicht.h"
#include "mapnode.h"
#include "exceptions.h"
#include "serialization.h"
#include "constants.h"
#include "mapblockobject.h"
#include "voxel.h"
#include "nodemetadata.h"
#include "staticobject.h"
#include "mapblock_nodemod.h"
#ifndef SERVER
	#include "mapblock_mesh.h"
#endif

class Map;

#define BLOCK_TIMESTAMP_UNDEFINED 0xffffffff

/*// Named by looking towards z+
enum{
	FACE_BACK=0,
	FACE_TOP,
	FACE_RIGHT,
	FACE_FRONT,
	FACE_BOTTOM,
	FACE_LEFT
};*/

enum ModifiedState
{
	// Has not been modified.
	MOD_STATE_CLEAN = 0,
	MOD_RESERVED1 = 1,
	// Has been modified, and will be saved when being unloaded.
	MOD_STATE_WRITE_AT_UNLOAD = 2,
	MOD_RESERVED3 = 3,
	// Has been modified, and will be saved as soon as possible.
	MOD_STATE_WRITE_NEEDED = 4,
	MOD_RESERVED5 = 5,
};

// NOTE: If this is enabled, set MapBlock to be initialized with
//       CONTENT_IGNORE.
/*enum BlockGenerationStatus
{
	// Completely non-generated (filled with CONTENT_IGNORE).
	BLOCKGEN_UNTOUCHED=0,
	// Trees or similar might have been blitted from other blocks to here.
	// Otherwise, the block contains CONTENT_IGNORE
	BLOCKGEN_FROM_NEIGHBORS=2,
	// Has been generated, but some neighbors might put some stuff in here
	// when they are generated.
	// Does not contain any CONTENT_IGNORE
	BLOCKGEN_SELF_GENERATED=4,
	// The block and all its neighbors have been generated
	BLOCKGEN_FULLY_GENERATED=6
};*/

#if 0
enum
{
	NODECONTAINER_ID_MAPBLOCK,
	NODECONTAINER_ID_MAPSECTOR,
	NODECONTAINER_ID_MAP,
	NODECONTAINER_ID_MAPBLOCKCACHE,
	NODECONTAINER_ID_VOXELMANIPULATOR,
};

class NodeContainer
{
public:
	virtual bool isValidPosition(v3s16 p) = 0;
	virtual MapNode getNode(v3s16 p) = 0;
	virtual void setNode(v3s16 p, MapNode & n) = 0;
	virtual u16 nodeContainerId() const = 0;

	MapNode getNodeNoEx(v3s16 p)
	{
		try{
			return getNode(p);
		}
		catch(InvalidPositionException &e){
			return MapNode(CONTENT_IGNORE);
		}
	}
};
#endif

/*
	MapBlock itself
*/

class MapBlock /*: public NodeContainer*/
{
public:
	MapBlock(Map *parent, v3s16 pos, bool dummy=false);
	~MapBlock();
	
	/*virtual u16 nodeContainerId() const
	{
		return NODECONTAINER_ID_MAPBLOCK;
	}*/
	
	const Map * getParent() const
	{
		return m_parent;
	}
	Map * getParent()
	{
		return m_parent;
	}

	void reallocate()
	{
		if(data != NULL)
			delete[] data;
		u32 l = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;
		data = new MapNode[l];
		for(u32 i=0; i<l; i++){
			//data[i] = MapNode();
			data[i] = MapNode(CONTENT_IGNORE);
		}
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}

	/*
		Flags
	*/

	bool isDummy() const
	{
		return (data == NULL);
	}
	void unDummify()
	{
		assert(isDummy());
		reallocate();
	}
	
	/*
		This is called internally or externally after the block is
		modified, so that the block is saved and possibly not deleted from
		memory.
	*/
	// DEPRECATED, use *Modified()
	void setChangedFlag()
	{
		//dstream<<"Deprecated setChangedFlag() called"<<std::endl;
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	// DEPRECATED, use *Modified()
	void resetChangedFlag() const
	{
		//dstream<<"Deprecated resetChangedFlag() called"<<std::endl;
		resetModified();
	}
	// DEPRECATED, use *Modified()
	bool getChangedFlag() const
	{
		//dstream<<"Deprecated getChangedFlag() called"<<std::endl;
		if(getModified() == MOD_STATE_CLEAN)
			return false;
		else
			return true;
	}
	
	// m_modified methods
	void raiseModified(u32 mod)
	{
		m_modified = MYMAX(m_modified, mod);
	}
	u32 getModified() const
	{
		return m_modified;
	}
	void resetModified() const
	{
		m_modified = MOD_STATE_CLEAN;
	}
	
	// is_underground getter/setter
	bool getIsUnderground() const
	{
		return is_underground;
	}
	void setIsUnderground(bool a_is_underground)
	{
		is_underground = a_is_underground;
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}

#ifndef SERVER
	void setMeshExpired(bool expired)
	{
		m_mesh_expired = expired;
	}
	
	bool getMeshExpired() const
	{
		return m_mesh_expired;
	}
#endif

	void setLightingExpired(bool expired)
	{
		m_lighting_expired = expired;
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	bool getLightingExpired() const
	{
		return m_lighting_expired;
	}

	bool isGenerated() const
	{
		return m_generated;
	}
	void setGenerated(bool b)
	{
		raiseModified(MOD_STATE_WRITE_NEEDED);
		m_generated = b;
	}

	bool isValid() const
	{
		if(m_lighting_expired)
			return false;
		if(data == NULL)
			return false;
		return true;
	}

	/*
		Position stuff
	*/

	const v3s16 &getPos() const
	{
		return m_pos;
	}
		
	v3s16 getPosRelative() const
	{
		return m_pos * MAP_BLOCKSIZE;
	}
		
	core::aabbox3d<s16> getBox() const
	{
		return core::aabbox3d<s16>(getPosRelative(),
				getPosRelative()
				+ v3s16(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE)
				- v3s16(1,1,1));
	}

	/*
		Regular MapNode get-setters
	*/
	
	bool isValidPosition(const v3s16 &p) const
	{
		if(data == NULL)
			return false;
		return (p.X >= 0 && p.X < MAP_BLOCKSIZE
				&& p.Y >= 0 && p.Y < MAP_BLOCKSIZE
				&& p.Z >= 0 && p.Z < MAP_BLOCKSIZE);
	}

	const MapNode &getNode(s16 x, s16 y, s16 z) const
	{
		if(data == NULL)
			throw InvalidPositionException();
		if(x < 0 || x >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(y < 0 || y >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(z < 0 || z >= MAP_BLOCKSIZE) throw InvalidPositionException();
		return data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x];
	}
	
	const MapNode &getNode(const v3s16 &p) const
	{
		return getNode(p.X, p.Y, p.Z);
	}
	
	MapNode getNodeNoEx(const v3s16 &p) const
	{
		try{
			return getNode(p.X, p.Y, p.Z);
		}catch(InvalidPositionException &e){
			return MapNode(CONTENT_IGNORE);
		}
	}
	
	void setNode(s16 x, s16 y, s16 z, const MapNode & n)
	{
		if(data == NULL)
			throw InvalidPositionException();
		if(x < 0 || x >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(y < 0 || y >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(z < 0 || z >= MAP_BLOCKSIZE) throw InvalidPositionException();
		data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	
	void setNode(const v3s16 &p, const MapNode & n)
	{
		setNode(p.X, p.Y, p.Z, n);
	}

	/*
		Non-checking variants of the above
	*/

	const MapNode &getNodeNoCheck(s16 x, s16 y, s16 z) const
	{
		if(data == NULL)
			throw InvalidPositionException();
		return data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x];
	}
	
	const MapNode &getNodeNoCheck(const v3s16 &p) const
	{
		return getNodeNoCheck(p.X, p.Y, p.Z);
	}
	
	void setNodeNoCheck(s16 x, s16 y, s16 z, const MapNode & n)
	{
		if(data == NULL)
			throw InvalidPositionException();
		data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	
	void setNodeNoCheck(const v3s16 &p, const MapNode & n)
	{
		setNodeNoCheck(p.X, p.Y, p.Z, n);
	}

	/*
		These functions consult the parent container if the position
		is not valid on this MapBlock.
	*/
	bool isValidPositionParent(const v3s16 &p) const;
	const MapNode &getNodeParent(const v3s16 &p) const;
	void setNodeParent(const v3s16 &p, const MapNode & n);
	MapNode getNodeParentNoEx(const v3s16 &p) const;

	void drawbox(s16 x0, s16 y0, s16 z0, s16 w, s16 h, s16 d,
			const MapNode &node)
	{
		for(u16 z=0; z<d; z++)
			for(u16 y=0; y<h; y++)
				for(u16 x=0; x<w; x++)
					setNode(x0+x, y0+y, z0+z, node);
	}

	/*
		Graphics-related methods
	*/
	
	/*// A quick version with nodes passed as parameters
	u8 getFaceLight(u32 daynight_ratio, MapNode n, MapNode n2,
			v3s16 face_dir);*/
	/*// A more convenient version
	u8 getFaceLight(u32 daynight_ratio, v3s16 p, v3s16 face_dir)
	{
		return getFaceLight(daynight_ratio,
				getNodeParentNoEx(p),
				getNodeParentNoEx(p + face_dir),
				face_dir);
	}*/
	u8 getFaceLight2(u32 daynight_ratio, v3s16 p, const v3s16 &face_dir) const
	{
		return getFaceLight(daynight_ratio,
				getNodeParentNoEx(p),
				getNodeParentNoEx(p + face_dir),
				face_dir);
	}
	
#ifndef SERVER // Only on client

#if 1
	/*
		Thread-safely updates the whole mesh of the mapblock.
		NOTE: Prefer generating the mesh separately and then using
		replaceMesh().
	*/
	void updateMesh(u32 daynight_ratio);
#endif
	// Replace the mesh with a new one
	void replaceMesh(scene::SMesh *mesh_new);
#endif
	
	// See comments in mapblock.cpp
	bool propagateSunlight(core::map<v3s16, bool> & light_sources,
			bool remove_light=false, bool *black_air_left=NULL);
	
	// Copies data to VoxelManipulator to getPosRelative()
	void copyTo(VoxelManipulator &dst) const;
	// Copies data from VoxelManipulator getPosRelative()
	void copyFrom(const VoxelManipulator &dst);

	/*
		MapBlockObject stuff
		DEPRECATED
	*/
	
	/*void serializeObjects(std::ostream &os, u8 version)
	{
		m_objects.serialize(os, version);
	}*/
	// If smgr!=NULL, new objects are added to the scene
	void updateObjects(std::istream &is, u8 version,
			scene::ISceneManager *smgr, u32 daynight_ratio)
	{
		m_objects.update(is, version, smgr, daynight_ratio);

		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	void clearObjects()
	{
		m_objects.clear();

		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	void addObject(MapBlockObject *object)
			throw(ContainerFullException, AlreadyExistsException)
	{
		m_objects.add(object);

		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	void removeObject(s16 id)
	{
		m_objects.remove(id);

		raiseModified(MOD_STATE_WRITE_NEEDED);
	}
	const MapBlockObject * getObject(s16 id) const
	{
		return m_objects.get(id);
	}
	MapBlockObject * getObject(s16 id)
	{
		return m_objects.get(id);
	}
	const JMutexAutoLock * getObjectLock() const
	{
		return m_objects.getLock();
	}

	/*
		Moves objects, deletes objects and spawns new objects
	*/
	void stepObjects(float dtime, bool server, u32 daynight_ratio);

	// origin is relative to block
	void getObjects(v3f origin, f32 max_d,
			core::array<DistanceSortedObject> &dest) const
	{
		m_objects.getObjects(origin, max_d, dest);
	}

	s32 getObjectCount() const
	{
		return m_objects.getCount();
	}

#ifndef SERVER // Only on client
	/*
		Methods for setting temporary modifications to nodes for
		drawing

		returns true if the mod was different last time
	*/
	bool setTempMod(const v3s16 &p, const NodeMod &mod)
	{
		/*dstream<<"setTempMod called on block"
				<<" ("<<p.X<<","<<p.Y<<","<<p.Z<<")"
				<<", mod.type="<<mod.type
				<<", mod.param="<<mod.param
				<<std::endl;*/
		JMutexAutoLock lock(m_temp_mods_mutex);

		return m_temp_mods.set(p, mod);
	}
	// Returns true if there was one
	bool getTempMod(const v3s16 &p, NodeMod *mod) const
	{
		JMutexAutoLock lock(m_temp_mods_mutex);

		return m_temp_mods.get(p, mod);
	}
	bool clearTempMod(const v3s16 &p)
	{
		JMutexAutoLock lock(m_temp_mods_mutex);

		return m_temp_mods.clear(p);
	}
	bool clearTempMods()
	{
		JMutexAutoLock lock(m_temp_mods_mutex);
		
		return m_temp_mods.clear();
	}
	void copyTempMods(NodeModMap &dst) const
	{
		JMutexAutoLock lock(m_temp_mods_mutex);
		m_temp_mods.copy(dst);
	}
#endif

	/*
		Update day-night lighting difference flag.
		
		Sets m_day_night_differs to appropriate value.
		
		These methods don't care about neighboring blocks.
		It means that to know if a block really doesn't need a mesh
		update between day and night, the neighboring blocks have
		to be taken into account. Use Map::dayNightDiffed().
	*/
	void updateDayNightDiff();

	bool dayNightDiffed() const
	{
		return m_day_night_differs;
	}

	/*
		Miscellaneous stuff
	*/
	
	/*
		Tries to measure ground level.
		Return value:
			-1 = only air
			-2 = only ground
			-3 = random fail
			0...MAP_BLOCKSIZE-1 = ground level
	*/
	s16 getGroundLevel(const v2s16 &p2d) const;

	/*
		Timestamp (see m_timestamp)
		NOTE: BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	void setTimestamp(u32 time)
	{
		m_timestamp = time;
		raiseModified(MOD_STATE_WRITE_AT_UNLOAD);
	}
	void setTimestampNoChangedFlag(u32 time)
	{
		m_timestamp = time;
	}
	u32 getTimestamp() const
	{
		return m_timestamp;
	}
	
	/*
		See m_usage_timer
	*/
	void resetUsageTimer()
	{
		m_usage_timer = 0;
	}
	void incrementUsageTimer(float dtime)
	{
		m_usage_timer += dtime;
	}
	u32 getUsageTimer() const
	{
		return m_usage_timer;
	}

	/*
		Serialization
	*/
	
	// These don't write or read version by itself
	void serialize(std::ostream &os, u8 version) const;
	void deSerialize(std::istream &is, u8 version);
	// Used after the basic ones when writing on disk (serverside)
	void serializeDiskExtra(std::ostream &os, u8 version) const;
	void deSerializeDiskExtra(std::istream &is, u8 version);

private:
	/*
		Private methods
	*/

	/*
		Used only internally, because changes can't be tracked
	*/

	MapNode & getNodeRef(s16 x, s16 y, s16 z)
	{
		if(data == NULL)
			throw InvalidPositionException();
		if(x < 0 || x >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(y < 0 || y >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(z < 0 || z >= MAP_BLOCKSIZE) throw InvalidPositionException();
		return data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x];
	}
	const MapNode & getNodeRef(s16 x, s16 y, s16 z) const
	{
		if(data == NULL)
			throw InvalidPositionException();
		if(x < 0 || x >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(y < 0 || y >= MAP_BLOCKSIZE) throw InvalidPositionException();
		if(z < 0 || z >= MAP_BLOCKSIZE) throw InvalidPositionException();
		return data[z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + y*MAP_BLOCKSIZE + x];
	}
	MapNode & getNodeRef(const v3s16 &p)
	{
		return getNodeRef(p.X, p.Y, p.Z);
	}
	const MapNode & getNodeRef(const v3s16 &p) const
	{
		return getNodeRef(p.X, p.Y, p.Z);
	}

public:
	/*
		Public member variables
	*/

#ifndef SERVER // Only on client
	scene::SMesh *mesh;
	mutable JMutex mesh_mutex;
#endif
	
	NodeMetadataList m_node_metadata;
	StaticObjectList m_static_objects;
	
private:
	/*
		Private member variables
	*/

	// NOTE: Lots of things rely on this being the Map
	Map *m_parent;
	// Position in blocks on parent
	v3s16 m_pos;
	
	/*
		If NULL, block is a dummy block.
		Dummy blocks are used for caching not-found-on-disk blocks.
	*/
	MapNode * data;

	/*
		- On the server, this is used for telling whether the
		  block has been modified from the one on disk.
		- On the client, this is used for nothing.
	*/
	mutable u32 m_modified;

	/*
		When propagating sunlight and the above block doesn't exist,
		sunlight is assumed if this is false.

		In practice this is set to true if the block is completely
		undeground with nothing visible above the ground except
		caves.
	*/
	bool is_underground;

	/*
		Set to true if changes has been made that make the old lighting
		values wrong but the lighting hasn't been actually updated.

		If this is false, lighting is exactly right.
		If this is true, lighting might be wrong or right.
	*/
	bool m_lighting_expired;
	
	// Whether day and night lighting differs
	bool m_day_night_differs;

	bool m_generated;
	
	// DEPRECATED
	MapBlockObjectList m_objects;

#ifndef SERVER // Only on client
	/*
		Set to true if the mesh has been ordered to be updated
		sometime in the background.
		In practice this is set when the day/night lighting switches.
	*/
	bool m_mesh_expired;
	
	// Temporary modifications to nodes
	// These are only used when drawing
	NodeModMap m_temp_mods;
	mutable JMutex m_temp_mods_mutex;
#endif
	
	/*
		When block is removed from active blocks, this is set to gametime.
		Value BLOCK_TIMESTAMP_UNDEFINED=0xffffffff means there is no timestamp.
	*/
	u32 m_timestamp;

	/*
		When the block is accessed, this is set to 0.
		Map will unload the block when this reaches a timeout.
	*/
	float m_usage_timer;
};

inline bool blockpos_over_limit(const v3s16 &p)
{
	return
	  (p.X < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.X >  MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Y < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Y >  MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Z < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
	|| p.Z >  MAP_GENERATION_LIMIT / MAP_BLOCKSIZE);
}

/*
	Returns the position of the block where the node is located
*/
inline v3s16 getNodeBlockPos(const v3s16 &p)
{
	return getContainerPos(p, MAP_BLOCKSIZE);
}

inline v2s16 getNodeSectorPos(const v2s16 &p)
{
	return getContainerPos(p, MAP_BLOCKSIZE);
}

inline s16 getNodeBlockY(s16 y)
{
	return getContainerPos(y, MAP_BLOCKSIZE);
}

/*
	Get a quick string to describe what a block actually contains
*/
std::string analyze_block(const MapBlock *block);

#endif

