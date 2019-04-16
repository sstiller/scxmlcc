#include <chrono>

#include "dot_output.h"
#include "version.h"

/*
 * Todo:
 *  Add a node containing the scxml content (instead of digraph label) including start actions, datamodel,...
 *  Add actions to final states
 * Problems:
 *   Arrow from compound state to itself
 *     https://stackoverflow.com/questions/46646181/connect-graphviz-cluster-to-itself
 */

void dot_output::gen()
{
    std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    out << indent << "// This file is automatically generated by scxmlcc (version " << version() << ")\n"
        << indent << "// For more information, see http://scxmlcc.org\n"
        << indent << "// View result online: https://dreampuf.github.io/GraphvizOnline/\n";

    out << indent << "digraph finite_state_machine {\n"
        << ++indent << "label=\"Document: " << htmlEscape(sc.sc().name) << "\\l"
        << indent << "Date: " << std::put_time(std::localtime(&currentTime), "%F") << "\\l\""
        << indent << "node [shape = Mrecord]\n"
        << indent << "compound=true\n"
        << indent << "size=\"8,5\"\n";

    gen_states();
    out << --indent << "}\n";
}

void dot_output::gen_states()
{
    // create initial state for the children of the document and connect it
    if(! sc.sc().initial.target.empty())
    {
        scxml_parser::state startState;
        startState.id = "_start";
        out << indent << "_start[label=\"\",shape=\"circle\",style=filled,fixedsize=\"true\",fillcolor=\"black\",width=\"0.2\"]\n";
        gen_transition(startState, sc.sc().initial, out);
    }

    const scxml_parser::plist<scxml_parser::state>& states = sc.sc().states;

    // states
    for(const auto& state : states)
    {
        gen_state(*state, out);
    }

    // transitions must be created after all states are in our "created" list.
    for(const auto& state : states)
    {
        for(const auto& transition : state->transitions)
        {
            gen_transition(*state, *transition, out);
        }
    }

}

void dot_output::gen_state(const scxml_parser::state &state, std::ostream& os)
{
    if(stateAdded(state.id))
    {
        return;
    }
    if(hasChildren(state))
    {
        gen_state_with_children(state, os);
        return;
    }

    if(! state.type)
    {
        gen_state_simple(state, os);
    }else
    {
        if(*state.type == "final")
        {
            gen_state_final(state, os);
            return;
        }
// #error and now? Unknown state type
    }
    addState(state.id);
}

void dot_output::gen_state_with_children(const scxml_parser::state &state, std::ostream &os)
{
    // Create a cluster
    int clusterNumber = currentClusterNumber++;
    os << indent << "subgraph cluster" << clusterNumber << " {\n"
       << ++indent << "style=\"rounded\"\n"
       << indent << "label=<\n"
       << ++indent << "<table border='0' cellborder='0' style='rounded'>\n"
       << ++indent << "<tr><td colspan='3'>";
    if(state.type)
    {
        os << "<i>" << state.type << "</i>";
    }
    os << "<b>" << state.id << "</b></td></tr>\n";

    gen_actions("entry", state.entry_actions, os);
    gen_actions("exit", state.exit_actions, os);

    os << --indent << "</table>\n"
       << --indent << ">\n";

    for(const auto& stateName : getChildrenNames(state))
    {
        std::ostringstream subOss;
        gen_state(getState(stateName), subOss);
        os << subOss.str();
    }

    // for some reason parallel states have an initial state, don't add it for them
    if(! state.type || *state.type != "parallel")
    {
        // create initial state of the children and connect it
        if(! state.initial.target.empty())
        {
            scxml_parser::state startState;
            startState.id = std::string("_start") + std::to_string(clusterNumber);
            os << indent << "_start" << clusterNumber << "[label=\"\",shape=\"circle\",style=filled,fixedsize=\"true\",fillcolor=\"black\",width=\"0.2\"]\n";
            gen_transition(startState, state.initial, os);
        }
    }


    os << --indent << "}\n";
    addState(state.id, clusterNumber);
}

void dot_output::gen_state_simple(const scxml_parser::state &state, std::ostream &os)
{
    os << indent << state.id << "[\n"
       << ++indent << "label=<\n"
       << ++indent << "<table border='0' cellborder='0' style='rounded'>\n"
       << ++indent << "<tr><td colspan='3'><b>" << state.id << "</b></td></tr>\n";

    gen_actions("entry", state.entry_actions, os);
    gen_actions("exit", state.exit_actions, os);

    os << --indent << "</table>\n"
       << --indent << ">\n"
       << --indent << "]\n";
    addState(state.id);
}

void dot_output::gen_state_final(const scxml_parser::state &state, std::ostream &os)
{
    os << indent << state.id
       << indent << "[label=\"\","
       << ++indent << "shape=doublecircle,"
       << indent << "style=filled,"
       << indent << "fixedsize=true,"
       << indent << "fillcolor=black,"
       << indent << "width=0.2]"
       << --indent << " // Final\n";
    addState(state.id);
}

void dot_output::gen_transition(const scxml_parser::state& sourceState,
                                const scxml_parser::transition& transition,
                                std::ostream& os)
{
    scxml_parser::slist targets = transition.target;
    if(targets.empty())
    {
        targets.push_back(sourceState.id);
    }
    for(const auto& target : targets)
    {
        std::string events;
        for(const auto& event : transition.event)
        {
            events += event + ",";
        }
        if(!events.empty())
        {
            events.pop_back();
        }


        const scxml_parser::state& targetState = getState(target);
        const scxml_parser::state& targetLeaf = getFirstLeafState(targetState);
        const scxml_parser::state& sourceLeaf = getFirstLeafState(sourceState);

        os << indent << sourceLeaf.id << "->" << targetLeaf.id;

        if((sourceState.id != sourceLeaf.id)
                || (targetState.id != targetLeaf.id)
                || transition.condition
                || !events.empty())
        {
            os << "[\n";
            ++indent;
            // transition needs label

            // draw arrow connections to the first child because arrow to cluster is not possible
            if(sourceState.id != sourceLeaf.id)
            {
                os << indent << "ltail=cluster" << getStateClusterNumber(sourceState.id) << ",\n";
            }
            if(targetState.id != targetLeaf.id)
            {
              os << indent << "lhead=cluster" << getStateClusterNumber(targetState.id) << ",\n";
            }

            if(transition.type && *transition.type == "internal")
            {
                os << indent << "style=\"dashed\",\n";
            }

            if(transition.condition || !events.empty())
            {
                os << indent << "label=<\n"
                   << ++indent << "<table border='0'>\n"
                   << ++indent << "<tr><td colspan='2'>" << events;
                if(transition.condition)
                {
                  os << " ["<< htmlEscape(*transition.condition) << "]";
                }
                os << "</td></tr>\n";

                gen_actions("onTrans", transition.actions, os);

                os << --indent << "</table>\n";
                os << --indent << ">\n";
            }
            os << --indent << "]";
        } // transition needs label
        os << indent << "\n";
    }
}

void dot_output::gen_actions(const std::string& actionLabel,
                             const scxml_parser::plist<scxml_parser::action>& actions,
                             std::ostream& os)
{

    if(actions.empty())
    {
        return;
    }
    // count number of needed rows
    int lineCount = 0;
    for(auto& action : actions)
    {
        lineCount += action->attr.size();
    }
    os << indent << "<tr><td rowspan='" << lineCount << "'>" << actionLabel << "</td>";

    bool firstActionAttr = true;
    for(auto& action : actions)
    {
        if(! firstActionAttr)
        {
            os << indent << "</tr>\n"
               << indent << "<tr>";
        }
        firstActionAttr = false;

        // type = script,...
        // first = expr, ...
        // second = content

        // TODO: More than one <td>/td> possible, this does not fit to the colspan stuff if more than one.
        for(auto& actionPair : action->attr)
        {
            os << "<td><i>" << action->type << ":" << actionPair.first << "</i></td>"
               << "<td border=\"1\">" << htmlEscape(actionPair.second) << "</td>";
            // htmlEscape
        }
    }
    os << indent << "</tr>\n";
}

bool dot_output::stateAdded(const std::string& stateName) const
{
    return addedStateNames.find(stateName) != addedStateNames.end();
}

bool dot_output::addState(const std::string& stateName, int clusterNumber)
{
    addedStateNames[stateName] = clusterNumber;
}

int dot_output::getStateClusterNumber(const std::string& stateName) const
{
    if(! stateAdded(stateName))
    {
        return 0;
    }
    return addedStateNames.at(stateName);
}

bool dot_output::hasChildren(const scxml_parser::state &state) const
{
    for(const auto& statePtr : sc.sc().states)
    {
        if(statePtr->parent)
        {
            if(statePtr->parent->id == state.id)
            {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> dot_output::getChildrenNames(const scxml_parser::state &state) const
{
    std::vector<std::string> ret;
    for(const auto& currStatePtr : sc.sc().states)
    {
        if(currStatePtr->parent)
        {
            if(currStatePtr->parent->id == state.id)
            {
                ret.push_back(currStatePtr->id);
            }
        }
    }
    return ret;
}

const scxml_parser::state& dot_output::getState(const std::string &stateName)
{
    for(const auto& currStatePtr : sc.sc().states)
    {
        if(currStatePtr->id == stateName)
        {
            return *currStatePtr;
        }
    }
    throw std::out_of_range(std::string("State name not found: ") + stateName);
}

const scxml_parser::state &dot_output::getFirstLeafState(const scxml_parser::state &state)
{
    //Find the first state which is child of the given state
    for(const auto& currStatePtr : sc.sc().states)
    {
        if(currStatePtr->parent)
        {
            if(currStatePtr->parent->id == state.id)
            {
                // found, try again with the found one
                return getFirstLeafState(*currStatePtr);
            }
        }
    }

    // if we are here, we have a leaf node
    return state;
}

std::string dot_output::htmlEscape(const std::string& data)
{
    std::string ret;
    ret.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos)
    {
        switch(data[pos])
        {
            case '&':  ret.append("&amp;");   break;
            case '\"': ret.append("&quot;");  break;
            case '\'': ret.append("&apos;");  break;
            case '<':  ret.append("&lt;");    break;
            case '>':  ret.append("&gt;");    break;
            case '\n': ret.append("<br align=\"left\"/>");   break;
//            case ' ':  ret.append("&nbsp;");  break;
            case '|':  ret.append("\\|");     break;
            case '{':  ret.append("\\{");     break;
            case '}':  ret.append("\\}");     break;
            default:   ret.append(&data[pos], 1); break;
        }
    }
    return ret;
}
