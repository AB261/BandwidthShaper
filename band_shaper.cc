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
 */

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/attribute.h"
#include "ns3/object-factory.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/net-device-queue-interface.h"
#include "tbf-queue-disc.h"
#include "band_shaper.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BandwidthShaper");

NS_OBJECT_ENSURE_REGISTERED (BandwidthShaper);

TypeId BandwidthShaper::GetTypeId (void)
{
  // TODO: Need to figure out all this stuff
  static TypeId tid = TypeId ("ns3::BandwidthShaper")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<BandwidthShaper> ()
    .AddAttribute ("MaxSize",
                   "The max queue size",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    // .AddAttribute ("Burst",
    //                "Size of the first bucket in bytes",
    //                UintegerValue (125000),
    //                MakeUintegerAccessor (&BandwidthShaper::SetBurst),
    //                MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Mtu",
                   "Size of the second bucket in bytes. If null, it is initialized"
                   " to the MTU of the receiving NetDevice (if any)",
                   UintegerValue (0),
                   MakeUintegerAccessor (&BandwidthShaper::SetMtu),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Rate",
                   "Rate at which tokens enter the first bucket in bps or Bps.",
                   DataRateValue (DataRate ("125KB/s")),
                   MakeDataRateAccessor (&BandwidthShaper::SetRate),
                   MakeDataRateChecker ())
    // .AddAttribute ("PeakRate",
    //                "Rate at which tokens enter the second bucket in bps or Bps."
    //                "If null, there is no second bucket",
    //                DataRateValue (DataRate ("0KB/s")),
    //                MakeDataRateAccessor (&BandwidthShaper::SetPeakRate),
    //                MakeDataRateChecker ())
    // .AddTraceSource ("TokensInFirstBucket",
    //                  "Number of First Bucket Tokens in bytes",
    //                  MakeTraceSourceAccessor (&BandwidthShaper::m_btokens),
    //                  "ns3::TracedValueCallback::Uint32")
    // .AddTraceSource ("TokensInSecondBucket",
    //                  "Number of Second Bucket Tokens in bytes",
    //                  MakeTraceSourceAccessor (&BandwidthShaper::m_ptokens),
    //                  "ns3::TracedValueCallback::Uint32")
  ;

  return tid;
}

BandwidthShaper::BandwidthShaper ()
  : QueueDisc (QueueDiscSizePolicy::SINGLE_CHILD_QUEUE_DISC)
{
  NS_LOG_FUNCTION (this);
}

BandwidthShaper::~BandwidthShaper ()
{
  NS_LOG_FUNCTION (this);
}

void
BandwidthShaper::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  QueueDisc::DoDispose ();
}

void
BandwidthShaper::SetMtu (uint32_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;
}

uint32_t
BandwidthShaper::GetMtu (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mtu;
}

void
BandwidthShaper::SetRate (DataRate rate)
{
  NS_LOG_FUNCTION (this << rate);
  m_rate = rate;
}

DataRate
BandwidthShaper::GetRate (void) const
{
  NS_LOG_FUNCTION (this);
  return m_rate;
}

bool
BandwidthShaper::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  // TODO: Need to figure out all the calculations for figuring out the overhead.
  uint32_t packetLength = item->GetLength();
  uint32_t netLength, adjustedLength;
  
  m_networkOffset = computeNetworkOffset(item); // TODO: Need to make this
  netLength = packetLength - m_networkOffset;

  m_overhead = computeOverhead(item); // TODO: Need to make this
  adjustedLength = netLength + m_overhead;

  m_isAtm = isAtmEnabled(); // TODO: Need to make this
  m_isPtm = isPtmEnabled(); // TODO: Need to make this

  // Calculations based on ATM ad PTM enabling according to paper
  if (m_isAtm)
    {
      adjustedLength = ceil(adjustedLength/48)*53;
    }
  else if (m_isPtm)
    {
      adjustedLength = ceil(adjustedLength/64)*65;
    }

  // Check if there is an backlog and if there arent any, check if the next
  // dequeue time is beyond now.
  if (m_backlog == 0 && Simulator::now.Compare(m_timeCheckPoint) == -1) 
    {
      m_timeCheckPoint = Simulator::now;
    }

  bool retval = GetQueueDiscClass (0)->GetQueueDisc ()->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the child queue
  // disc because QueueDisc::AddQueueDiscClass sets the drop callback

  NS_LOG_LOGIC ("Current queue size: " << GetNPackets () << " packets, " << GetNBytes () << " bytes");

  return retval;
}


Ptr<QueueDiscItem>
BandwidthShaper::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);
  Ptr<const QueueDiscItem> itemPeek = GetQueueDiscClass (0)->GetQueueDisc ()->Peek ();

  if (itemPeek)
    {
      uint32_t pktSize = itemPeek->GetSize ();
      NS_LOG_LOGIC ("Next packet size " << pktSize);

      Time now = Simulator::Now ();

      double delta = (now  - m_timeCheckPoint).GetSeconds ();
      NS_LOG_LOGIC ("Time Difference delta " << delta);
      // The Timer setup.
      /* If delta > 0 that means the time checkpoint has passed and so 
      the packet can be dequeued. If that is not the case then set up the required
      delay to wake up the simulator at the time and send the packet.  */
      if(delta>0){
        Ptr<QueueDiscItem> item = GetQueueDiscClass (0)->GetQueueDisc ()->Dequeue ();
        Time nextDelayTime = now+pktSize*(m_shapedBandwidth.GetBitRate()/8);
        m_id = Simulator::Schedule (nextDelayTime, &QueueDisc::Run, this);
        return item;
      }else{
        Time requiredDelayTime = m_timeCheckPoint-now;
        m_id = Simulator::Schedule (requiredDelayTime, &QueueDisc::Run, this);
        NS_LOG_LOGIC("Waking Event Scheduled in " << requiredDelayTime);
      }

    }
  return 0;
}

bool
BandwidthShaper::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("BandwidthShaper cannot have internal queues");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("BandwidthShaper cannot have packet filters");
      return false;
    }

  if (GetNQueueDiscClasses () == 0)
    {
      // create a FIFO queue disc
      ObjectFactory factory;
      factory.SetTypeId ("ns3::FifoQueueDisc");
      Ptr<QueueDisc> qd = factory.Create<QueueDisc> ();

      if (!qd->SetMaxSize (GetMaxSize ()))
        {
          NS_LOG_ERROR ("Cannot set the max size of the child queue disc to that of BandwidthShaper");
          return false;
        }
      qd->Initialize ();

      Ptr<QueueDiscClass> c = CreateObject<QueueDiscClass> ();
      c->SetQueueDisc (qd);
      AddQueueDiscClass (c);
    }

  if (GetNQueueDiscClasses () != 1)
    {
      NS_LOG_ERROR ("BandwidthShaper needs 1 child queue disc");
      return false;
    }

  // This type of variable initialization would normally be done in
  // InitializeParams (), but we want to use the value to subsequently
  // check configuration of peak rate, so we move it forward here.
  if (m_mtu == 0)
    {
      Ptr<NetDeviceQueueInterface> ndqi = GetNetDeviceQueueInterface ();
      Ptr<NetDevice> dev;
      // if the NetDeviceQueueInterface object is aggregated to a
      // NetDevice, get the MTU of such NetDevice
      if (ndqi && (dev = ndqi->GetObject<NetDevice> ()))
        {
          m_mtu = dev->GetMtu ();
        }
    }

  if (m_mtu == 0 && m_peakRate > DataRate ("0bps"))
    {
      NS_LOG_ERROR ("A non-null peak rate has been set, but the mtu is null. No packet will be dequeued");
      return false;
    }

  if (m_burst <= m_mtu)
    {
      NS_LOG_WARN ("The size of the first bucket (" << m_burst << ") should be "
                    << "greater than the size of the second bucket (" << m_mtu << ").");
    }

  if (m_peakRate > DataRate ("0bps") && m_peakRate <= m_rate)
    {
      NS_LOG_WARN ("The rate for the second bucket (" << m_peakRate << ") should be "
                    << "greater than the rate for the first bucket (" << m_rate << ").");
    }

  return true;
}

void
BandwidthShaper::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  // Initialising other variables to 0.
  m_timeCheckPoint = Seconds (0);
  m_id = EventId ();
}

} // namespace ns3
