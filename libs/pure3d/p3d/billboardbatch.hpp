#ifndef P3D_BILLBOARD_BATCH_HPP
#define P3D_BILLBOARD_BATCH_HPP
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_QUAD_STREAM)
namespace p3dBillboardBatch {
constexpr unsigned MaxGroups=16,MaxQuads=128;
extern bool enabled,compact;
inline unsigned MinimumGroups(){return compact?4u:2u;}
extern unsigned batches,groups,avoided,reservationFailures;
}
#endif
#endif
