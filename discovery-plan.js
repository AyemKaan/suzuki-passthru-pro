'use strict';
function buildDiscoveryPlan(catalog,identity,selections){
  const attributes=new Map(catalog.tables.vehicle_attribute.map(a=>[String(a.id),a]));
  const eligible=catalog.discoveryLists.filter(list=>{
    const key=list.selection;
    return [1,2].includes(key.discovery_type)&&Number(key.rel_ecu_discovery_id)===1&&Number(key.rel_base_id)===Number(identity.baseId)&&(!key.cid||key.cid===identity.cid)&&(!key.part_number||key.part_number===identity.partNumber);
  });
  const rank=list=>(list.selection.cid?2:0)+(list.selection.part_number?1:0);
  const best=Math.max(-1,...eligible.map(rank));
  const lists=eligible.filter(list=>rank(list)===best).map(list=>{
    let unknown=false;
    for(const [group,id] of selections){
      const values=list.attributes.map(a=>attributes.get(String(a.rel_vehicle_attribute_id))).filter(a=>a&&String(a.rel_vehicle_attribute_title_id)===String(group));
      if(!values.length){unknown=true;continue}
      if(!values.some(a=>String(a.id)===String(id)||a.wildcard===true||a.wildcard==='true'))return null;
    }
    return {...list,attributeEvidence:unknown?'incomplete':'compatible'};
  }).filter(Boolean);
  const modules=new Map();
  for(const list of lists)for(const entry of list.sequence){
    if(!modules.has(entry.baseId))modules.set(entry.baseId,{...entry,sourceLists:[]});
    modules.get(entry.baseId).sourceLists.push(list.selection.id);
  }
  return {executionVerified:false,vehicleVerified:false,genericKey:best<=0,selectedAttributes:Object.fromEntries(selections),listIds:lists.map(l=>l.selection.id),lists,modules:[...modules.values()].sort((a,b)=>a.element.order-b.element.order)};
}
if(typeof module!=='undefined')module.exports={buildDiscoveryPlan};
