/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RESOURCE_HANDOVER_POLICY_H
#define RESOURCE_HANDOVER_POLICY_H
/* Each consumer is balanced independently, even when all share one rail. */
static int resource_counts_valid(unsigned int own_n,int opp_n,
                                 const unsigned long long *handles,
                                 const int *counts,unsigned int enabled)
{
 unsigned int i,j;
 if(own_n!=2 || opp_n!=2 || enabled>1)return 0;
 for(i=0;i<4;i++) {
  if(!handles[i] || counts[i]<0 || counts[i]>1)return 0;
  for(j=0;j<i;j++)if(handles[i]==handles[j])return 0;
 }
 return 1;
}
#endif
