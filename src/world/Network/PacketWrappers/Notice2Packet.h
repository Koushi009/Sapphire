#pragma once

#include <Network/GamePacket.h>
#include "Forwards.h"

namespace Sapphire::Network::Packets::WorldPackets::Server
{

  /**
  * @brief Packet to display a quest specific info message.
  */
  class Notice2Packet : public ZoneChannelPacket< FFXIVIpcNotice2 >
  {
  public:
    Notice2Packet( uint32_t entityId, uint32_t questId, int8_t msgId, uint8_t numOfArgs = 0, uint32_t var1 = 0, uint32_t var2 = 0 ) :
      ZoneChannelPacket< FFXIVIpcNotice2 >( entityId, entityId )
    {
      initialize( questId, msgId, numOfArgs, var1, var2 );
    };

  private:
    void initialize( uint32_t questId, int8_t msgId, uint8_t numOfArgs, uint32_t var1, uint32_t var2 )
    {
      m_data.handlerId = questId;
      m_data.noticeId = msgId;
      // todo: not correct
      m_data.numOfArgs = numOfArgs;
      m_data.args[0] = var1;
      m_data.args[1] = var2;
    };
  };

}