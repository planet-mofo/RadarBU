// RadarBU.cpp

#include "bzfsAPI.h"
#include "StateDatabase.h"
#include "../src/bzfs/bzfs.h"

int checkRange(int min, int max, int amount) {
    int num = 0;
    if ((amount >= min) && (amount <= max)) {
        num = 1;
    } else if ((amount < min) || (amount > max)) {
        num = 0;
    } else {
        num = -1;
    }
    return num;
}

int checkPlayerSlot(int player) {
    return checkRange(0,199,player); // 199 because of array.
}

bool checkIfValidPlayer(int player) {
    bool valid = false;
    bz_BasePlayerRecord *playerRec = bz_getPlayerByIndex ( player );
    if (playerRec) {
        valid = true;
    }
    bz_freePlayerRecord(playerRec);
    return valid;
}

// Basic utility functions


class RadarBU : public bz_Plugin
{
   public:
   const char* Name(){return "RadarBU";}
   void Init ( const char* /*config*/ );
   void Event(bz_EventData *eventData );
   void Cleanup ( void );
   int grabbed[200];
   // In theory, we can easily skip this, since we only need to have
};

void RadarBU::Init (const char* commandLine) {
    for(int i=0;i<=199;i++){
        grabbed[i]=0;
        // We shouldn't have any issues as this is a custom flag plug-in,
        //  hence no players should be on the server while the plug-in is loaded.
    }
   Register(bz_ePlayerJoinEvent);
   Register(bz_ePlayerPartEvent);
   Register(bz_ePlayerDieEvent);
   Register(bz_eBZDBChange);
   Register(bz_eFlagGrabbedEvent);
   Register(bz_eFlagDroppedEvent);
   Register(bz_eFlagTransferredEvent);
   //^ normally thief event entirely unneeded
   Register(bz_eCaptureEvent);

   bz_registerCustomBZDBDouble("_radarBU", bz_getBZDBDouble("_worldSize"));
}

void RadarBU::Cleanup (void) {
   bz_removeCustomBZDBVariable("_radarBU");
   Flush();
}

static void sendPlayerBZDB(int playerID, const std::string& key,
                                         const std::string& value)
{
  void *bufStart = getDirectMessageBuffer();
  void *buf = nboPackUShort(bufStart, 1);
  buf = nboPackUByte (buf, key.length());
  buf = nboPackString(buf, key.c_str(), key.length());
  buf = nboPackUByte (buf, value.length());
  buf = nboPackString(buf, value.c_str(), value.length());

  const int len = (char*)buf - (char*)bufStart;
  directMessage(playerID, MsgSetVar, len, bufStart);
}


static void sendPlayerBZDB(int playerID, const std::string& key,
                                         const float value)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%f", value);
  sendPlayerBZDB(playerID, key, buf);
}

void RadarBU::Event(bz_EventData *eventData ) {
   switch (eventData->eventType) {
    case bz_ePlayerJoinEvent: {
      bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
      int player = joinData->playerID;
      if (checkPlayerSlot(player)==1) {
        grabbed[player]=0;
      }
        
        
    }break;

    case bz_ePlayerPartEvent: {
      bz_PlayerJoinPartEventData_V1* partData = (bz_PlayerJoinPartEventData_V1*)eventData;
      int player = partData->playerID;
      if (checkPlayerSlot(player)==1) {
        grabbed[player]=0;
      }
        
    }break;

      case bz_ePlayerDieEvent: {
      //int playerID=((bz_PlayerDieEventData_V2*)eventData)->playerID;
      bz_PlayerDieEventData_V2* deathData = (bz_PlayerDieEventData_V2*)eventData;
      int player = deathData->playerID;
      //int killer = deathData->killerID;
      if (checkPlayerSlot(player)==1) {
        if (grabbed[player]==1) {
            sendPlayerBZDB(player,"_radarLimit", bz_getBZDBDouble("_radarLimit"));
            grabbed[player]=0;
        }
      }
    }break;

    case bz_eBZDBChange: {
      bz_BZDBChangeData_V1* bzdbData = (bz_BZDBChangeData_V1*)eventData;
      if (bzdbData->key == "_radarRange") {
        //radarRange = atof(bzdbData->key.c_str());
        //@TODO some radar update if we have any radar flag holders.
        for(int i=0; i<=199;i++) {
            if (grabbed[i] == 1) {
                if (checkIfValidPlayer(i) == true) {
                    sendPlayerBZDB(i,"_radarLimit", (bz_getBZDBDouble("_radarBU") * 4));
                }
            }
        }
      } else {
        if (bzdbData->key == "_radarLimit") {
          //@TODO test this:
          // We do not know if we need to give a delay and in that case, we register tick event.
          for(int i=0; i<=199;i++) {
            if (grabbed[i] == 1) {
                if (checkIfValidPlayer(i) == true) {
                    sendPlayerBZDB(i,"_radarLimit", (bz_getBZDBDouble("_radarBU") * 4));
                }
            }
          }
        }
      } 
    }break;

    case bz_eFlagGrabbedEvent: {
      bz_FlagGrabbedEventData_V1* flagGrabData = (bz_FlagGrabbedEventData_V1*)eventData;
      int player = flagGrabData->playerID;
      if (checkPlayerSlot(player)==1) {
          if (strcmp(flagGrabData->flagType, "BU") == 0) {
            sendPlayerBZDB(player,"_radarLimit", (bz_getBZDBDouble("_radarBU") * 4));
            grabbed[player]=1;// This indicator is very important for smoothly setting values.
          }
      }   
    }break;

    case bz_eFlagDroppedEvent: {
      bz_FlagDroppedEventData_V1* flagDropData = (bz_FlagDroppedEventData_V1*)eventData;
      int player = flagDropData->playerID;
      if (checkPlayerSlot(player)==1) {
          if (strcmp(flagDropData->flagType, "BU") == 0) {
            sendPlayerBZDB(flagDropData->playerID,"_radarLimit", bz_getBZDBDouble("_radarLimit"));
            grabbed[player]=0; // They no longer have the flag, we ignore them in updates.
          }
      }
    }break;

    case bz_eFlagTransferredEvent: {
      bz_FlagTransferredEventData_V1* thiefData = (bz_FlagTransferredEventData_V1*)eventData;
      int from = thiefData->fromPlayerID;
      int to   = thiefData->toPlayerID;
      //@NOTE: shouldn't happen in almost every case, but if so.
      if (strcmp(thiefData->flagType, "BU") == 0) {
        sendPlayerBZDB(from,"_radarLimit", bz_getBZDBDouble("_radarLimit"));
        if (checkPlayerSlot(from)==1) {
            grabbed[from]=0;
        }
        
        sendPlayerBZDB(to,"_radarLimit", (bz_getBZDBDouble("_radarBU") * 4));
        if (checkPlayerSlot(to)==1) {
            grabbed[to]=1;
        }
      }
    }break;

    case bz_eCaptureEvent: {
      bz_CTFCaptureEventData_V1* capData = (bz_CTFCaptureEventData_V1*)eventData;
      //@TODO add more advanced check/handling.
      // (There was some issue, but I don't quite remember exactly what it was with the cap event.)
      // Not related to above, but according to the code, we can't simply check for dead players.
      // Seems we got a less efficient option, but it works.
      bz_eTeamType teamCapped = capData->teamCapped;
      bz_APIIntList *playerList = bz_newIntList();
      if (playerList) {
      bz_getPlayerIndexList(playerList);
      for ( unsigned int i = 0; i < playerList->size(); i++) {
      int player = (*playerList)[i];
      bz_eTeamType playerTeam = bz_getPlayerTeam(player);
          if (playerTeam == teamCapped) {
            if (checkPlayerSlot(player)==1) {
                if (grabbed[player]==1) {
                    sendPlayerBZDB(player,"_radarLimit", bz_getBZDBDouble("_radarLimit"));
                    grabbed[player]=0;
                }
            }
          }
      }
      bz_deleteIntList(playerList);
      }
    }break;

    default:{ 
    }break;
  }
}

BZ_PLUGIN(RadarBU)

