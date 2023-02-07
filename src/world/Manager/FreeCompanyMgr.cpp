#include <iterator>

#include <Common.h>
#include <Exd/ExdData.h>
#include <Service.h>

#include <Logging/Logger.h>
#include <Database/DatabaseDef.h>
#include <Manager/ChatChannelMgr.h>
#include <Network/GamePacket.h>

#include "FreeCompany/FreeCompany.h"
#include "FreeCompanyMgr.h"

#include "Actor/Player.h"

#include "WorldServer.h"

#include <Network/GameConnection.h>
#include <Network/PacketDef/Zone/ServerZoneDef.h>
#include <Network/PacketWrappers/FreeCompanyResultPacket.h>
#include <Network/PacketDef/ClientIpcs.h>

#include "Session.h"

using namespace Sapphire;
using namespace Sapphire::Network::Packets;
using namespace Sapphire::Network::Packets::WorldPackets::Server;
using namespace Sapphire::World::Manager;

bool FreeCompanyMgr::loadFreeCompanies()
{
  auto& db = Common::Service< Db::DbWorkerPool< Db::ZoneDbConnection > >::ref();
  auto& chatChannelMgr = Common::Service< Manager::ChatChannelMgr >::ref();

  auto query = db.getPreparedStatement( Db::FC_SEL_ALL );
  auto res = db.query( query );

  /* FreeCompanyId, MasterCharacterId, FcName, FcTag, FcCredit, FcCreditAccumu, FcRank, FcPoint, CrestId, CreateDate, GrandCompanyID, "
                    "       ReputationList, FcStatus, FcBoard, FcMotto, ActiveActionList, ActiveActionLeftTimeList, StockActionList */
  while( res->next() )
  {
    uint64_t fcId = res->getUInt64( 1 );
    uint64_t masterId = res->getUInt64( 2 );
    std::string name = res->getString( 3 );
    std::string tag = res->getString( 4 );
    uint64_t credit = res->getUInt64( 5 );
    uint64_t creditAcc = res->getUInt64( 6 );
    uint8_t rank = res->getUInt8( 7 );
    uint64_t points = res->getUInt64( 8 );
    uint64_t crestId = res->getUInt64( 9 );
    uint32_t creationDate = res->getUInt( 10 );
    uint8_t gcId = res->getUInt8( 11 );
    auto reputationListVec = res->getBlobVector( 12 );
    uint8_t status = res->getUInt8( 13 );
    std::string board = res->getString( 14 );
    std::string motto = res->getString( 15 );


    auto chatChannelId = chatChannelMgr.createChatChannel( Common::ChatChannelType::FreeCompanyChat );

    auto fcPtr = std::make_shared< FreeCompany >( fcId, name, tag, masterId, chatChannelId );
    m_fcIdMap[ fcId ] = fcPtr;
    m_fcNameMap[ name ] = fcPtr;

    fcPtr->setCredit( credit );
    fcPtr->setCreditAccumulated( creditAcc );
    fcPtr->setRank( rank );
    fcPtr->setPoints( points );
    fcPtr->setCrest( crestId );
    fcPtr->setCreateDate( creationDate );
    fcPtr->setGrandCompany( gcId );
    fcPtr->setFcStatus( static_cast< Common::FreeCompanyStatus >( status ) );

  }

  return true;
}

void FreeCompanyMgr::writeFreeCompany( uint64_t fcId )
{
  auto& db = Common::Service< Db::DbWorkerPool< Db::ZoneDbConnection > >::ref();

  auto fc = getFreeCompanyById( fcId );

  if( !fc )
  {
    Logger::error( "FreeCompany {} not found for write!", fcId );
  }

  auto query = db.getPreparedStatement( Db::FC_UP );

  /*  MasterCharacterId = ?, FcName = ?, FcTag = ?, FcCredit = ?, FcCreditAccumu = ?,
   *  FcRank = ?, FcPoint = ?, ReputationList = ?, CrestId = ?,"
   *  CreateDate = ?, GrandCompanyID = ?, FcStatus = ?, FcBoard = ?, "
   *  FcMotto = ?, ActiveActionList = ?, , ActiveActionLeftTimeList = ?, StockActionList = ? "
   */

  query->setUInt64( 1, fc->getMasterId() );
  query->setString( 2, fc->getName() );
  query->setString( 3, fc->getTag() );
  query->setUInt64( 4, fc->getCredit() );
  query->setUInt64( 5, fc->getCreditAccumulated() );
  query->setUInt( 6, fc->getRank() );
  query->setUInt64( 7, fc->getPoints() );
  std::vector< uint8_t > repList( 24 );
  query->setBinary( 8, repList );
  query->setUInt64( 9, fc->getCrest() );
  query->setUInt( 10, fc->getCreateDate() );
  query->setUInt( 11, fc->getGrandCompany() );
  query->setUInt( 12, static_cast< uint8_t >( fc->getFcStatus() ) );
  query->setString( 13, fc->getFcBoard() );
  query->setString( 14, fc->getFcMotto() );
  std::vector< uint8_t > activeActionList( 24 );
  query->setBinary( 15, activeActionList );
  std::vector< uint8_t > activeActionLeftTimeList( 24 );
  query->setBinary( 16, activeActionLeftTimeList );
  std::vector< uint8_t > stockActionList( 120 );
  query->setBinary( 17, stockActionList );

  query->setInt64( 18, static_cast< int64_t >( fc->getId() ) );
  db.execute( query );

}

FreeCompanyPtr FreeCompanyMgr::getFcByName( const std::string& name )
{
  auto it = m_fcNameMap.find( name );
  if( it == m_fcNameMap.end() )
    return nullptr;
  else
    return it->second;
}

FreeCompanyPtr FreeCompanyMgr::getFreeCompanyById( uint64_t fcId )
{
  auto it = m_fcIdMap.find( fcId );
  if( it == m_fcIdMap.end() )
    return nullptr;
  else
    return it->second;
}

FreeCompanyPtr FreeCompanyMgr::createFreeCompany( const std::string& name, const std::string& tag, Entity::Player& player )
{
  uint64_t freeCompanyId = 1;

  if( !m_fcIdMap.empty() )
  {
    auto lastIdx = ( --m_fcIdMap.end() )->first;
    freeCompanyId = lastIdx + 1;
  }

  // check if a fc with the same name already exists
  auto lsIt = m_fcNameMap.find( name );
  if( lsIt != m_fcNameMap.end() )
    return nullptr;

  auto& chatChannelMgr = Common::Service< Manager::ChatChannelMgr >::ref();
  auto chatChannelId = chatChannelMgr.createChatChannel( Common::ChatChannelType::FreeCompanyChat );
  chatChannelMgr.addToChannel( chatChannelId, player );

  uint64_t masterId = player.getCharacterId();
  Logger::debug( "MasterID# {}", masterId );

  uint32_t createDate = Common::Util::getTimeSeconds();

  auto fcPtr = std::make_shared< FreeCompany >( freeCompanyId, name, tag, masterId, chatChannelId );
  fcPtr->setCreateDate( createDate );
  fcPtr->setGrandCompany( player.getGc() );
  fcPtr->setFcStatus( Common::FreeCompanyStatus::InviteStart );
  fcPtr->setRank( 1 );
  m_fcIdMap[ freeCompanyId ] = fcPtr;
  m_fcNameMap[ name ] = fcPtr;

  //FreeCompanyId, MasterCharacterId, FcName, FcTag, FcCredit, FcCreditAccumu, FcRank, FcPoint,
  //ReputationList, CrestId, CreateDate, GrandCompanyID, FcStatus, FcBoard, FcMotto
  auto& db = Common::Service< Db::DbWorkerPool< Db::ZoneDbConnection > >::ref();
  auto stmt = db.getPreparedStatement( Db::ZoneDbStatements::FC_INS );
  stmt->setUInt64( 1, freeCompanyId );
  stmt->setUInt64( 2, masterId );
  stmt->setString( 3, std::string( name ) );
  stmt->setString( 4, std::string( tag ) );
  stmt->setUInt64( 5, 0 );
  stmt->setUInt64( 6, 0 );
  stmt->setUInt( 7, 1 );
  stmt->setUInt64( 8, 0 );
  std::vector< uint8_t > rep( 24 );
  stmt->setBinary( 9, rep );
  stmt->setUInt64( 10, 0 );
  stmt->setUInt( 11, createDate );
  stmt->setUInt( 12, fcPtr->getGrandCompany() );
  stmt->setUInt( 13, static_cast< uint8_t >( Common::FreeCompanyStatus::InviteStart ) );
  stmt->setString( 14, std::string( "" ) );
  stmt->setString( 15, std::string( "" ) );

  db.directExecute( stmt );

  auto& server = Common::Service< World::WorldServer >::ref();

  auto fcResult = makeFcResult( player, freeCompanyId,
                                2, FreeCompanyResultPacket::ResultType::Create,
                                0, FreeCompanyResultPacket::UpdateStatus::Execute,
                                fcPtr->getName(), fcPtr->getTag() );

  server.queueForPlayer( player.getCharacterId(), fcResult );

  return fcPtr;
}

void FreeCompanyMgr::sendFreeCompanyStatus( Entity::Player& player )
{
  auto& server = Common::Service< World::WorldServer >::ref();

  auto fcStatusResult = makeZonePacket< FFXIVIpcGetFcStatusResult >( player.getId() );

  auto playerFc = getPlayerFreeCompany( player );
  if( !playerFc )
    return;

  fcStatusResult->data().AuthorityList = 0;
  fcStatusResult->data().ChannelID = playerFc->getChatChannel();
  fcStatusResult->data().Param = 2; // this appears to control which packets are requested afterwards
  fcStatusResult->data().CharaFcParam = 0;
  fcStatusResult->data().CrestID = playerFc->getCrest();
  fcStatusResult->data().FcRank = playerFc->getRank();
  fcStatusResult->data().FcStatus = static_cast< uint8_t >( playerFc->getFcStatus() );
  fcStatusResult->data().FreeCompanyID = playerFc->getId();
  fcStatusResult->data().GrandCompanyID = playerFc->getGrandCompany();

  server.queueForPlayer( player.getCharacterId(), fcStatusResult );

}

FreeCompanyPtr FreeCompanyMgr::getPlayerFreeCompany( Entity::Player& player ) const
{
  for( const auto &[ key, value ] : m_fcIdMap )
  {
    if( value->getMasterId() == player.getCharacterId() )
    {
      return value;
    }
  }
  return nullptr;
}

void FreeCompanyMgr::sendFcInviteList( Entity::Player& player )
{
  auto fc = getPlayerFreeCompany( player );
  if( !fc )
    return;

  auto& server = Common::Service< World::WorldServer >::ref();

  auto inviteListPacket = makeZonePacket< FFXIVIpcGetFcInviteListResult >( player.getId() );
  inviteListPacket->data().GrandCompanyID = fc->getGrandCompany();
  inviteListPacket->data().FreeCompanyID = fc->getId();
  std::strcpy( inviteListPacket->data().FcTag, fc->getTag().c_str() );
  std::strcpy( inviteListPacket->data().FreeCompanyName, fc->getName().c_str() );

  // fill master character data
  auto masterCharacter = server.getPlayer( fc->getMasterId() );
  if( !masterCharacter )
    Logger::error( "FreeCompanyMgr: Unable to look up master character#{}!", fc->getMasterId() );

  inviteListPacket->data().MasterCharacter.GrandCompanyID = masterCharacter->getGc();
  inviteListPacket->data().MasterCharacter.CharacterID = masterCharacter->getCharacterId();
  strcpy( inviteListPacket->data().MasterCharacter.CharacterName, masterCharacter->getName().c_str() );
  inviteListPacket->data().MasterCharacter.SelectRegion = masterCharacter->getSearchSelectRegion();
  inviteListPacket->data().MasterCharacter.OnlineStatus = masterCharacter->getOnlineStatusMask();
  inviteListPacket->data().MasterCharacter.GrandCompanyRank[ 0 ] = masterCharacter->getGcRankArray()[ 0 ];
  inviteListPacket->data().MasterCharacter.GrandCompanyRank[ 1 ] = masterCharacter->getGcRankArray()[ 1 ];
  inviteListPacket->data().MasterCharacter.GrandCompanyRank[ 2 ] = masterCharacter->getGcRankArray()[ 2 ];

  // todo - fill invite characters

  server.queueForPlayer( player.getCharacterId(), inviteListPacket );
}

void FreeCompanyMgr::sendFcStatus( Entity::Player& player )
{
  auto fc = getPlayerFreeCompany( player );
  auto fcResultPacket = makeZonePacket< FFXIVIpcGetFcStatusResult >( player.getId() );
  auto& resultData = fcResultPacket->data();
  resultData.CharaFcParam = 1;

  if( fc )
  {
    resultData.FreeCompanyID = fc->getId();
    resultData.AuthorityList = 0;
    resultData.HierarchyType = 0;
    resultData.GrandCompanyID = fc->getGrandCompany();
    resultData.FcRank = fc->getRank();
    resultData.CrestID = fc->getCrest();
    resultData.FcStatus = static_cast< uint8_t >( fc->getFcStatus() );
    resultData.ChannelID = fc->getChatChannel();
    resultData.CharaFcParam = 0;
  }

  auto& server = Common::Service< World::WorldServer >::ref();
  server.queueForPlayer( player.getCharacterId(), fcResultPacket );
}

void FreeCompanyMgr::onFcLogin( uint64_t characterId )
{
  auto& server = Common::Service< World::WorldServer >::ref();
  auto player = server.getPlayer( characterId );
  if( !player )
    return;

  auto fc = getPlayerFreeCompany( *player );
  if( !fc )
    return;

  auto fcResult = makeFcResult( *player, fc->getId(),
                                2, FreeCompanyResultPacket::ResultType::FcLogin,
                                0, FreeCompanyResultPacket::UpdateStatus::Execute,
                                fc->getName(), fc->getTag() );

  server.queueForPlayer( player->getCharacterId(), fcResult );

  // todo - send packet to rest of fc members
}
