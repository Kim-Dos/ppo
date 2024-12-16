#pragma once

typedef unsigned char BYTE;

constexpr unsigned int SERVERPORT = 4000;

//const char* Lobby_IP = "127.0.0.1";

const int MAXSIZE = 1024;
const int MAXUSER = 500;
const int RoomCodeLen = 14;

const int ipsize = 16;
const int NumOfGameServer = 2;


enum All_Packet_Type {
	// Client -> Lobby 
	CL_LOGIN,
	CL_LOGOUT,
	CL_QUICK_MATCHING,
	CL_ENTER_ROOM_CODE,
	CL_CREATE_ROOM,
	CL_START_GAME,
	// Client -> Game
	CG_MOVEMENT,
	CG_ATTACK,

	// Lobby -> Client
	LC_LOG_INFO, // 로그인과 대비
	LC_PUSH_MATCHING_Q, // 퀵매칭하려고 방 생성 후 오토매칭
	LC_FIND_ROOM_CODE, // 룸코드보고 입장
	LC_ROOM_CREATE, // 방 생성
	LC_COMPLETE_SESSION, //게임서버로 이제 갈 준비해라 및 게임서버 IP보내줘야함

	// Lobby -> Game
	LG_ROOMINFO,		//방을 생성하고, 입력된 정보를 통해 클라이언트 인원들을 받아라
	LG_REFAIRROOM,		// (서버하나 다운시) 다른 서버에서있는 정보를 읽어와서 너가 기존게임을 이어가라
	
	// Game -> Lobby
	GL_SERVERAMOUNT, // 서버에 얼만큼 차있는지
	GL_ENDGAME,		//서버에서 게임이 끝났다. ( == 클라이언트 정보들을 가져가라)
	// Game -> Client
	GC_MOVEMENT,
	GC_CHECKATTACK,
	GC_LINKGAMESERVER
};

enum Skilltype {
	NormalSkill,
	UltimitSkill
};

enum charactertype {
	Attacker,
	Sniper,
	Assister
};

enum stagetype {
	first,
	second,
	third
};

struct FXYZ {
	float x, y, z;
};



struct ButtonPack {
	void* packet;
	BYTE type;
};

struct MoveData {
	FXYZ position;
	FXYZ velocity;
	FXYZ look;
	char act;
};



#pragma pack(push, 1)


//--------------Client  -->> LobbyServer ---------------
struct CLLobbyLogin
{
	BYTE size;
	BYTE type;
	char name[20];
};


struct CLLobbyLogOut
{
	BYTE size;
	BYTE type;
	char name[20];
};


struct CLClickMatching
{
	BYTE size;
	BYTE type;
	BYTE stageNumber;
};


struct CLEnterRoomCode
{
	BYTE size;
	BYTE type;
	char RoomCode[RoomCodeLen];
};


struct CLCreateRoom
{
	BYTE size;	
	BYTE type;
	BYTE stageNumber;
};


struct CLStartGame {
	BYTE size;
	BYTE type;
	char RoomCode[RoomCodeLen];
};

//------------------Client -->> GameServer -----------------

struct CGAttack {
	BYTE size;
	BYTE type;
	char act_type;
	FXYZ LookVector;
	FXYZ position;
};

struct CGMove {
	BYTE size;
	BYTE type;
	char RoomCode[RoomCodeLen];
	MoveData movedata;
	int play_timer;
	int partynumber;
};

struct CGLinkInfo {
	BYTE size;
	BYTE type;
	char RoomCode[RoomCodeLen];
	int userID;
};



//-------------- LobbyServer  -->> Client ---------------

struct LCLogInfo {
	BYTE size;
	BYTE type;
	bool exist;
};


struct LCPuahMatchingQ
{
	BYTE size;
	BYTE type;
};


struct LCFindRoomCode {

	BYTE size;
	BYTE type;
	bool Fulled;
	bool exist;
};


struct LCRoomCreate
{
	BYTE size;
	BYTE type;
	char RoomCode[RoomCodeLen];
};

struct LCCompleteGameSession {
	BYTE size;
	BYTE type;
	int userNumber;
	char GameServerIP[ipsize];
	int ServerPort;
};


//---------------LobbyServer -> GameServer  --------------
struct LGLobbyToGame
{
	BYTE size;
	BYTE type;
	char RoomCode[RoomCodeLen];
	unsigned char stage_number;
	int usernumber1;
	int usernumber2;
	int usernumber3;
	int usernumber4;
};

struct LGRefair {

};

//---------------- GameServer -->> LobbyServer -----------------
struct GLServerAmount {
	BYTE size;
	BYTE type;
	size_t amount;
};


//---------------- GameServer -->> Clients -----------------

struct GCPlayerMove {
	BYTE size;
	BYTE type;
	int partynumber;
	MoveData movedtata;
};

struct GCMonster {
	BYTE size;
	BYTE type;
	FXYZ pos;
	char act;

	//send changing model data?
};



#pragma pack(pop)