#pragma once
#include "eiem_source_adaptation_jobs.h"
#include "eiem_compiled_mod_writer.h"

// Shared CPU compiler for one current source. It neither invokes an author
// executable nor commits disk/Unity state. The caller owns catalog resolution,
// transactional persistence and dependency-group delivery.
namespace EiemSourceAdaptationCompiler {
using namespace EiemNativeInput;
inline EiemConsumerUpdateRecords::Stamp StampFor(const Package& author,const EiemStaticSources::Selection& selection,const std::string& source){
 Require(selection.candidates.count(source),"adaptation source absent from current candidate set");std::set<std::string> required{source};
 auto add=[&](const std::string& path){if(!path.empty())required.insert(selection.assets.at(EiemStaticSources::Logical(path)));};
 for(const auto& target:author.targetIndex)add(target.first.path);for(const auto& material:author.materials)add(material.second.source.path);for(const auto& texture:author.textures)add(texture.second.source.path);
 for(const auto& resource:author.resources)if(resource.second->blocks.count("@source-prefabs")){auto rows=Strings(*resource.second,"@source-prefabs");Require(rows.size()%2==0,"adaptation Physics source identity truncated");for(size_t i=0;i<rows.size();i+=2)add(rows[i]);}
 required=EiemStaticSources::Closure(selection,std::move(required));required.erase(source);EiemOfflinePackage::Writer dependencies;
 for(const auto& name:required){dependencies.S(name);dependencies.S(selection.inputs.at(name).revision);}
 return {selection.inputs.at(source).revision,EiemResourceReloadPlan::Hash(dependencies.b),InputContractDigest(author),"compiled-view+exact-mesh+root-owner"};
}
inline EiemSourceAdaptationJobs::Result Compile(const Package& author,const EiemStaticSources::Selection& current,const std::filesystem::path& game,const EiemSourceAdaptationJobs::Request& request){
 Require(request.stamp==StampFor(author,current,request.source),"adaptation request no longer describes current source/author dependencies");
 auto selection=current;selection.candidates={request.source};std::set<Identity> targets;for(const auto& row:author.targetIndex)targets.insert({EiemStaticSources::Logical(row.first.path),row.first.asset});
 auto discovery=EiemStaticSources::FindConsumers(selection,targets,false);Require(discovery.decisions.size()==1,"single-source adaptation examined other candidates");
 EiemSourceAdaptationJobs::Result result;result.decision=discovery.decisions.at(request.source);result.decision.stamp=request.stamp;
 if(result.decision.roots.empty())return result;
 auto artifact=std::make_shared<EiemCompiledMod::Artifact>(EiemCompiledModWriter::Compile(author,selection,game,{},nullptr,&discovery));
 const auto& entry=selection.inputs.at(request.source);auto identity=request.source+"\n"+entry.revision;auto file=EiemResourceReloadPlan::Hash(Bytes(identity.begin(),identity.end()))+".vfs";
 for(size_t i=0;i<artifact->manifest.plan.originals.size();++i)if(artifact->manifest.plan.originals[i].file==file){result.input=i;break;}
 Require(result.input!=SIZE_MAX,"adapted source missing from compiled input views");result.view=std::shared_ptr<const EiemCompiledInputView::Recipe>(artifact,&artifact->views.at(result.input));result.artifact=std::move(artifact);return result;
}
} // namespace EiemSourceAdaptationCompiler
