/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "ActivePeerConnectionCommand.h"
#include "PeerInitiateConnectionCommand.h"
#include "message.h"
#include "DownloadEngine.h"
#include "BtContext.h"
#include "PeerStorage.h"
#include "PieceStorage.h"
#include "BtRuntime.h"
#include "Peer.h"
#include "Logger.h"
#include "prefs.h"
#include "Option.h"
#include "BtConstants.h"
#include "SocketCore.h"
#include "BtAnnounce.h"
#include "RequestGroup.h"

namespace aria2 {

ActivePeerConnectionCommand::ActivePeerConnectionCommand
(int cuid,
 RequestGroup* requestGroup,
 DownloadEngine* e,
 const BtContextHandle& btContext,
 time_t interval)
  :
  Command(cuid),
  _requestGroup(requestGroup),
  _btContext(btContext),
  interval(interval),
  e(e),
  _numNewConnection(5)
{
  _requestGroup->increaseNumCommand();
}

ActivePeerConnectionCommand::~ActivePeerConnectionCommand()
{
  _requestGroup->decreaseNumCommand();
}

bool ActivePeerConnectionCommand::execute() {
  if(_btRuntime->isHalt()) {
    return true;
  }
  if(checkPoint.elapsed(interval)) {
    checkPoint.reset();
    TransferStat tstat = _requestGroup->calculateStat();
    const unsigned int maxDownloadLimit =
      _requestGroup->getMaxDownloadSpeedLimit();
    const unsigned int maxUploadLimit = _requestGroup->getMaxUploadSpeedLimit();
    unsigned int thresholdSpeed =
      _requestGroup->getOption()->getAsInt(PREF_BT_REQUEST_PEER_SPEED_LIMIT);
    if(maxDownloadLimit > 0) {
      thresholdSpeed = std::min(maxDownloadLimit, thresholdSpeed);
    }
    if(// for seeder state
       (_pieceStorage->downloadFinished() && _btRuntime->lessThanMaxPeers() &&
	(maxUploadLimit == 0 || tstat.getUploadSpeed() < maxUploadLimit*0.8)) ||
       // for leecher state
       (!_pieceStorage->downloadFinished() &&
	(tstat.getDownloadSpeed() < thresholdSpeed ||
	 _btRuntime->lessThanMinPeers()))) {

      unsigned int numConnection = 0;
      if(_pieceStorage->downloadFinished()) {
	if(_btRuntime->getMaxPeers() > _btRuntime->getConnections()) {
	  numConnection =
	    std::min(_numNewConnection,
		     _btRuntime->getMaxPeers()-_btRuntime->getConnections());
	}
      } else {
	numConnection = _numNewConnection;
      }

      for(unsigned int numAdd = numConnection;
	  numAdd > 0 && _peerStorage->isPeerAvailable(); --numAdd) {
	PeerHandle peer = _peerStorage->getUnusedPeer();
	connectToPeer(peer);
      }
      if(_btRuntime->getConnections() == 0 &&
	 !_pieceStorage->downloadFinished()) {
	_btAnnounce->overrideMinInterval(BtAnnounce::DEFAULT_ANNOUNCE_INTERVAL);
      }
    }
  }
  e->commands.push_back(this);
  return false;
}

void ActivePeerConnectionCommand::connectToPeer(const PeerHandle& peer)
{
  if(peer.isNull()) {
    return;
  }
  peer->usedBy(e->newCUID());
  PeerInitiateConnectionCommand* command =
    new PeerInitiateConnectionCommand(peer->usedBy(), _requestGroup, peer, e,
				      _btContext, _btRuntime);
  command->setPeerStorage(_peerStorage);
  command->setPieceStorage(_pieceStorage);
  e->commands.push_back(command);
  logger->info(MSG_CONNECTING_TO_PEER,
	       cuid, peer->ipaddr.c_str());
}

void ActivePeerConnectionCommand::setBtRuntime
(const SharedHandle<BtRuntime>& btRuntime)
{
  _btRuntime = btRuntime;
}

void ActivePeerConnectionCommand::setPieceStorage
(const SharedHandle<PieceStorage>& pieceStorage)
{
  _pieceStorage = pieceStorage;
}

void ActivePeerConnectionCommand::setPeerStorage
(const SharedHandle<PeerStorage>& peerStorage)
{
  _peerStorage = peerStorage;
}

void ActivePeerConnectionCommand::setBtAnnounce
(const SharedHandle<BtAnnounce>& btAnnounce)
{
  _btAnnounce = btAnnounce;
}

} // namespace aria2
