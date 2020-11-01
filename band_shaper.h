/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Kungliga Tekniska Högskolan
 *               2017 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Cake Bandwidth Shaper
 *
 */
#ifndef BS_QUEUE_DISC_H
#define BS_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/event-id.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * \brief A BandwidthShaper packet queue disc
 */
class BandwidthShaper : public QueueDisc
{
public:

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief BandwidthShaper Constructor
   *
   * Create a Bandwidth queue disc
   */
  BandwidthShaper ();

  /**
   * \brief Destructor
   *
   * Destructor
   */
  virtual ~BandwidthShaper ();

  /**
    */
  void SetMtu (uint32_t mtu);

  /**
    */
  uint32_t GetMtu (void) const;

  /**
    */
  void SetRate (DataRate rate);

  /**
    */
  DataRate GetRate (void) const;


protected:
  /**
   * \brief Dispose of the object
   */
  virtual void DoDispose (void);

private:

  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  /* parameters for the Bandwidth Shaper */
  uint32_t m_mtu;        //!< Packet Size
  DataRate m_rate;       //!< Data rate of link layer


  /* variables stored by Bandwidth Shaper */
  Time m_timeCheckPoint;           //!< Time check-point
  EventId m_id;                    //!< EventId of the scheduled queue
  uint32_t m_overhead;             //!< actual size of a packet on the wire
  uint32_t m_networkOffset;
  bool m_isAtm;                    //!< true if is ATM network
  bool m_isPtm;                    //!< true if is PTM network
  uint32_t m_backlog; 
  DataRate m_shapedBandwidth;      //!< Bandwidth to be shaped to               
};

} // namespace ns3

#endif /* Bandwidth Shaper */
