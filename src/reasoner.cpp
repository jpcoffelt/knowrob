/*
 * Copyright (c) 2022, Daniel Beßler
 * All rights reserved.
 *
 * This file is part of KnowRob, please consult
 * https://github.com/knowrob/knowrob for license details.
 */

#include <knowrob/logging.h>
#include <knowrob/reasoner.h>
#include <knowrob/prolog/PrologReasoner.h>
#include <knowrob/mongolog/MongologReasoner.h>
#include <knowrob/swrl/SWRLReasoner.h>
#include <knowrob/esg/ESGReasoner.h>
// shared libraries
#include <dlfcn.h>

#include <utility>

using namespace knowrob;

/******************************************/
/*************** IReasoner ****************/
/******************************************/

void IReasoner::addDataFileHandler(const std::string &format, const DataFileLoader &fn)
{
	dataFileHandler_[format] = fn;
}

bool IReasoner::loadDataFile(const DataFilePtr &dataFile)
{
	if(dataFile->hasUnknownFormat()) {
		return loadDataFileWithUnknownFormat(dataFile);
	}
	else {
		auto it = dataFileHandler_.find(dataFile->format());
		if(it == dataFileHandler_.end()) {
			KB_WARN("Ignoring data file with unknown format \"{}\"", dataFile->format());
			return false;
		}
		else {
			KB_INFO("Using data file {} with format \"{}\".", dataFile->path(), dataFile->format());
			return it->second(dataFile);
		}
	}
}

/******************************************/
/************ ReasonerManager *************/
/******************************************/

ReasonerManager::ReasonerManager()
: reasonerIndex_(0)
{
	// add some default factory functions to create reasoner instances
	// TODO: can be done automatically in InitKnowledgeBase?
	addReasonerFactory("Mongolog", std::make_shared<TypedReasonerFactory<MongologReasoner>>("Mongolog"));
	addReasonerFactory("Prolog",   std::make_shared<TypedReasonerFactory<PrologReasoner>>("Prolog"));
	addReasonerFactory("ESG",      std::make_shared<TypedReasonerFactory<ESGReasoner>>("ESG"));
	addReasonerFactory("SWRL",     std::make_shared<TypedReasonerFactory<SWRLReasoner>>("SWRL"));
}

void ReasonerManager::loadReasoner(const boost::property_tree::ptree &config)
{
	auto lib = config.get_optional<std::string>("lib");
	auto type = config.get_optional<std::string>("type");
	auto name = config.get_optional<std::string>("name");

	// get a reasoner factory
	std::shared_ptr<ReasonerFactory> factory;
	if(lib.has_value()) {
		// use factory in DLL
		factory = loadReasonerPlugin(lib.value());
	}
	else if(type.has_value()) {
		// map type name to a factory
		const auto &it = reasonerFactories_.find(type.value());
		if(it == reasonerFactories_.end()) {
			KB_WARN("no factory registered for reasoner type '{}'.", type.value());
		}
		else {
			factory = it->second;
		}
	}
	else {
		KB_WARN("missing 'type' or 'lib' key in reasoner config.");
	}
	// make sure factory was found above
	if(!factory) {
		throw ReasonerError("failed to load a reasoner.");
	}
	// create a reasoner id, or use name property
	std::string reasonerID;
	if(name.has_value()) {
		reasonerID = name.value();
	}
	else {
		reasonerID = factory->name() + std::to_string(reasonerIndex_);
	}
	KB_INFO("Using reasoner `{}` with type `{}`.", reasonerID, factory->name());
	// create a new reasoner instance
	auto reasoner = factory->createReasoner(reasonerID);
	ReasonerConfiguration reasonerConfig;
	reasonerConfig.loadPropertyTree(config);
	if(!reasoner->loadConfiguration(reasonerConfig)) {
		KB_WARN("Reasoner `{}` failed to loadConfiguration.", reasonerID);
	}
	else {
		addReasoner(reasoner);
	}
	// increase reasonerIndex_
	reasonerIndex_ += 1;
}

std::shared_ptr<ReasonerPlugin> ReasonerManager::loadReasonerPlugin(const std::string &path)
{
	// TODO: map path to absolute path to avoid loading DLLs multiple times.
	auto it = loadedPlugins_.find(path);
	if(it == loadedPlugins_.end()) {
		auto p = std::make_shared<ReasonerPlugin>(path);
		auto jt = loadedPlugins_.insert(std::pair<std::string, std::shared_ptr<ReasonerPlugin>>(path, p));
		if(jt.first->second->loadDLL()) {
			return jt.first->second;
		}
	}
	else if(it->second->isLoaded()) {
		return it->second;
	}
	KB_WARN("Failed to open reasoner library at path '{}'.", path);
	return {};
}

void ReasonerManager::addReasonerFactory(const std::string &typeName, const std::shared_ptr<ReasonerFactory> &factory)
{
	reasonerFactories_[typeName] = factory;
}

void ReasonerManager::addReasoner(const std::shared_ptr<IReasoner> &reasoner)
{
	reasonerPool_.push_back(reasoner);
}

void ReasonerManager::removeReasoner(const std::shared_ptr<IReasoner> &reasoner)
{
	reasonerPool_.remove(reasoner);
}

std::list<std::shared_ptr<IReasoner>> ReasonerManager::getReasonerForPredicate(const PredicateIndicator &predicate)
{
	std::list<std::shared_ptr<IReasoner>> out;
	for(auto &x : reasonerPool_) {
		if(x->isCurrentPredicate(predicate)) {
			out.push_back(x);
		}
	}
	return out;
}

/******************************************/
/************ ReasonerPlugin **************/
/******************************************/

ReasonerPlugin::ReasonerPlugin(std::string dllPath)
		: handle_(nullptr),
		  create_(nullptr),
		  get_name_(nullptr),
		  dllPath_(std::move(dllPath))
{
}

ReasonerPlugin::~ReasonerPlugin()
{
	if(handle_) {
		dlclose(handle_);
		handle_ = nullptr;
	}
}

bool ReasonerPlugin::isLoaded()
{
	return (create_ != nullptr && get_name_ != nullptr);
}

bool ReasonerPlugin::loadDLL()
{
	handle_ = dlopen(dllPath_.c_str(), RTLD_LAZY);
	if(handle_ != nullptr) {
		create_ = (std::shared_ptr<IReasoner> (*)(const std::string&))
				dlsym(handle_, "knowrob_createReasoner");
		get_name_ = (char* (*)())
				dlsym(handle_, "knowrob_getPluginName");
		return isLoaded();
	}
	else {
		return false;
	}
}

std::shared_ptr<IReasoner> ReasonerPlugin::createReasoner(const std::string &reasonerID)
{
	return create_(reasonerID);
}

/******************************************/
/********* ReasonerConfiguration **********/
/******************************************/

void ReasonerConfiguration::loadPropertyTree(const boost::property_tree::ptree &config)
{
	static const std::string formatDefault = {};

	// load all key-value pairs into settings list
	for(const auto& key_val : config) {
		auto key_t = std::make_shared<StringTerm>(key_val.first);
		if(key_val.second.empty()) {
			settings.emplace_back(key_t, std::make_shared<StringTerm>(key_val.second.get_value<std::string>()));
		}
		else {
			loadSettings(key_t, key_val.second);
		}
	}

	auto data_sources = config.get_child_optional("data-sources");
	if(data_sources) {
		for(const auto &pair : data_sources.value()) {
			auto &subtree = pair.second;

			auto fileValue = subtree.get_optional<std::string>("file");
			if(fileValue.has_value()) {
				auto fileFormat = subtree.get("format",formatDefault);
				dataFiles.push_back(std::make_shared<DataFile>(fileValue.value(), fileFormat));
			}
			else {
				KB_WARN("Ignoring data source without \"file\" key.");
			}
		}
	}
}

void ReasonerConfiguration::loadSettings(const TermPtr &key1, const boost::property_tree::ptree &ptree)
{
	static auto colon_f = std::make_shared<PredicateIndicator>(":", 2);

	for(const auto& key_val : ptree) {
		if(key_val.first.empty()) {
			// this indicates a list
			// TODO: handle list values here
			KB_WARN("todo: handle list values in options");
			continue;
		}
		else {
			auto key2 = std::make_shared<StringTerm>(key_val.first);
			auto key_t = std::make_shared<Predicate>(Predicate(colon_f, { key1, key2 }));

			if(key_val.second.empty()) {
				settings.emplace_back(key_t, std::make_shared<StringTerm>(key_val.second.get_value<std::string>()));
			}
			else {
				loadSettings(key_t, key_val.second);
			}
		}
	}
}
